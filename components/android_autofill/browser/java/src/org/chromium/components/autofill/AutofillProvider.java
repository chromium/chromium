// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.components.autofill.AutofillRequest.FocusField;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

/**
 * This class works with Android autofill service to fill web form, it doesn't use Chrome's autofill
 * service or suggestion UI. All methods are supposed to be called in UI thread.
 *
 * <p>AutofillProvider handles one autofill session at time, each call of startAutofillSession
 * cancels previous session and starts a new one, the calling of other methods shall associate with
 * current session.
 *
 * <p>This class doesn't have 1:1 mapping to native AndroidAutofillProvider; the normal ownership
 * model is that this object is owned by the embedder-specific Java WebContents wrapper (e.g.,
 * AwContents.java in //android_webview), and AndroidAutofillProvider is owned by the
 * embedder-specific C++ WebContents wrapper (e.g., native AwContents in //android_webview).
 */
@JNINamespace("autofill")
public class AutofillProvider {
    /**
     * Factory interface for testing. AutofillManagerWrapper must be created in AutofillProvider
     * constructor.
     */
    public static interface AutofillManagerWrapperFactoryForTesting {
        AutofillManagerWrapper create(Context context);
    }

    private static AutofillManagerWrapperFactoryForTesting sAutofillManagerFactoryForTesting;

    private final String mProviderName;
    private AutofillManagerWrapper mAutofillManager;
    private ViewGroup mContainerView;
    private WebContents mWebContents;

    private AutofillRequest mRequest;
    private long mNativeAutofillProvider;
    private AutofillProviderUMA mAutofillUMA;
    private AutofillManagerWrapper.InputUIObserver mInputUIObserver;
    private long mAutofillTriggeredTimeMillis;
    private Context mContext;
    private AutofillPopup mDatalistPopup;
    private AutofillSuggestion[] mDatalistSuggestions;
    private WebContentsAccessibility mWebContentsAccessibility;
    private View mAnchorView;
    private PrefillRequest mPrefillRequest;
    // Whether onProvideAutofillVirtualStructure has been called for the current PrefillRequest.
    // Used solely for metrics.
    private boolean mStructureProvidedForPrefillRequest;

    public AutofillProvider(
            Context context,
            ViewGroup containerView,
            WebContents webContents,
            String providerName) {
        mWebContents = webContents;
        mProviderName = providerName;
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped("AutofillProvider.constructor")) {
            if (sAutofillManagerFactoryForTesting != null) {
                mAutofillManager = sAutofillManagerFactoryForTesting.create(context);
            } else {
                mAutofillManager = new AutofillManagerWrapper(context);
            }
            mContainerView = containerView;
            mAutofillUMA =
                    new AutofillProviderUMA(
                            context,
                            mAutofillManager.isAwGCurrentAutofillService(),
                            mAutofillManager.getPackageName());
            mInputUIObserver =
                    new AutofillManagerWrapper.InputUIObserver() {
                        @Override
                        public void onInputUIShown() {
                            // Not need to report suggestion window displayed if there is no live
                            // autofill session.
                            if (mRequest == null) return;
                            mAutofillUMA.onSuggestionDisplayed(
                                    System.currentTimeMillis() - mAutofillTriggeredTimeMillis);
                        }
                    };
            mAutofillManager.addInputUIObserver(mInputUIObserver);
            mContext = context;
        }
        initializeNativeAutofillProvider(webContents);
    }

    public void destroy() {
        mAutofillUMA.recordSession();
        detachFromJavaAutofillProvider();
        mAutofillManager.destroy();
    }

    /**
     * Invoked when container view is changed.
     *
     * @param containerView new container view.
     */
    public void onContainerViewChanged(ViewGroup containerView) {
        mContainerView = containerView;
    }

    /**
     * Invoked when autofill service needs the form structure.
     *
     * @param structure see View.onProvideAutofillVirtualStructure()
     * @param flags see View.onProvideAutofillVirtualStructure()
     */
    public void onProvideAutoFillVirtualStructure(ViewStructure structure, int flags) {
        // This method could be called for the session started by the native
        // control outside of the scope of autofill, e.g. the URL bar, in this case, we simply
        // return.
        if (mRequest == null && mPrefillRequest == null) return;

        Bundle bundle = structure.getExtras();
        if (bundle != null) {
            bundle.putCharSequence("VIRTUAL_STRUCTURE_PROVIDER_NAME", mProviderName);
            bundle.putCharSequence(
                    "VIRTUAL_STRUCTURE_PROVIDER_VERSION", VersionConstants.PRODUCT_VERSION);

            if (mRequest != null && mRequest.getAutofillHintsService() != null) {
                bundle.putBinder(
                        "AUTOFILL_HINTS_SERVICE", mRequest.getAutofillHintsService().getBinder());
            }
        }
        // We should have one of them available here, we start with AutofillRequest as it should be
        // available only if we started a session.
        FormData form;
        short focusFieldIndex = -1;
        if (mRequest != null) {
            form = mRequest.getForm();
            focusFieldIndex =
                    mRequest.getFocusField() != null ? mRequest.getFocusField().fieldIndex : -1;
            mAutofillUMA.onVirtualStructureProvided();
        } else {
            form = mPrefillRequest.getForm();
            mStructureProvidedForPrefillRequest = true;
        }
        form.fillViewStructure(structure, focusFieldIndex);
        if (AutofillManagerWrapper.isLoggable()) {
            AutofillManagerWrapper.log(
                    "onProvideAutoFillVirtualStructure fields:" + structure.getChildCount());
        }
    }

    /**
     * Invoked when autofill value is available, AutofillProvider shall fill the form with the
     * provided values.
     *
     * @param values the array of autofill values, the key is virtual id of form field.
     */
    public void autofill(final SparseArray<AutofillValue> values) {
        if (mNativeAutofillProvider != 0 && mRequest != null && mRequest.autofill(values)) {
            autofill(mNativeAutofillProvider);
            if (AutofillManagerWrapper.isLoggable()) {
                AutofillManagerWrapper.log("autofill values:" + values.size());
            }
            mAutofillUMA.onAutofill();
        }
    }

    /** @return whether query autofill suggestion. */
    public boolean shouldQueryAutofillSuggestion() {
        return mRequest != null
                && mRequest.getFocusField() != null
                && !mAutofillManager.isAutofillInputUIShowing();
    }

    public void queryAutofillSuggestion() {
        if (shouldQueryAutofillSuggestion()) {
            FocusField focusField = mRequest.getFocusField();
            mAutofillManager.requestAutofill(
                    mContainerView,
                    mRequest.getFieldVirtualId(focusField.fieldIndex),
                    focusField.absBound);
        }
    }

    public static void setAutofillManagerWrapperFactoryForTesting(
            AutofillManagerWrapperFactoryForTesting factory) {
        sAutofillManagerFactoryForTesting = factory;
        ResettersForTesting.register(() -> sAutofillManagerFactoryForTesting = null);
    }

    public void replaceAutofillManagerWrapperForTesting(AutofillManagerWrapper wrapper) {
        mAutofillManager = wrapper;
    }

    /**
     * Sends a prefill (cache) request to the Android Autofill Framework.
     *
     * @param form the form to send the prefill request for.
     */
    @CalledByNative
    public void sendPrefillRequest(FormData form) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.UPSIDE_DOWN_CAKE) return;
        // Return early if there's a session running already.
        if (mRequest != null && mRequest.getFocusField() != null) {
            return;
        }

        transformFormFieldToContainViewCoordinates(form);
        mPrefillRequest = new PrefillRequest(form);
        mStructureProvidedForPrefillRequest = false;

        mAutofillManager.notifyVirtualViewsReady(mContainerView, mPrefillRequest.getPrefillHints());
    }

    /**
     * Invoked when filling form is need. AutofillProvider shall ask autofill service for the values
     * with which to fill the form.
     *
     * @param formData the form needs to fill.
     * @param focus the index of focus field in formData
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     * @param hasServerPrediction whether the server prediction arrived.
     */
    @CalledByNative
    public void startAutofillSession(
            FormData formData,
            int focus,
            float x,
            float y,
            float width,
            float height,
            boolean hasServerPrediction) {
        Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
        if (mRequest != null) notifyViewExitBeforeDestroyRequest();

        // Check focusField inside short value? Autofill Manager might have session that wasn't
        // started by AutofillProvider, we just always cancel existing session here.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            mAutofillManager.cancel();
        }

        transformFormFieldToContainViewCoordinates(formData);
        mAutofillUMA.onSessionStarted(mAutofillManager.isDisabled());
        mRequest =
                new AutofillRequest(
                        formData, new FocusField((short) focus, absBound), hasServerPrediction);
        if (maybeShowBottomSheet(focus)) {
            mAutofillUMA.onBottomSheetShown();
        } else {
            notifyVirtualViewEntered(mContainerView, focus, absBound);
        }
        if (hasServerPrediction) {
            mAutofillUMA.onServerTypeAvailable(formData, /* afterSessionStarted= */ false);
        }
        mAutofillTriggeredTimeMillis = System.currentTimeMillis();

        mAutofillManager.notifyNewSessionStarted(hasServerPrediction);
    }

    /**
     * Attempts to show a bottom sheet if the Android version is U+ and there has been a prefill
     * request for the form in mRequest.
     *
     * @param focus the index of the focused field in mRequest.
     * @return whether the bottom sheet was shown.
     */
    boolean maybeShowBottomSheet(int focus) {
        if (Build.VERSION.SDK_INT < VERSION_CODES.UPSIDE_DOWN_CAKE) return false;
        if (mPrefillRequest == null
                || mPrefillRequest.getForm().mSessionId != mRequest.getForm().mSessionId) {
            return false;
        }

        boolean bottomSheetShown = showAutofillDialog(mContainerView, focus);
        if (mNativeAutofillProvider != 0) {
            AutofillProviderJni.get()
                    .onShowBottomSheetResult(
                            mNativeAutofillProvider,
                            bottomSheetShown,
                            mStructureProvidedForPrefillRequest);
        }
        return bottomSheetShown;
    }

    /**
     * Invoked when form field's value is changed.
     *
     * @param index index of field in current form.
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     *
     */
    @CalledByNative
    public void onFormFieldDidChange(int index, float x, float y, float width, float height) {
        // Check index inside short value?
        if (mRequest == null) return;

        short sIndex = (short) index;
        FocusField focusField = mRequest.getFocusField();
        if (focusField == null || sIndex != focusField.fieldIndex) {
            onFocusChangedImpl(true, index, x, y, width, height, /* causedByValueChange= */ true);
        } else {
            // Currently there is no api to notify both value and position
            // change, before the API is available, we still need to call
            // notifyVirtualViewEntered() to tell current coordinates because
            // the position could be changed.
            Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
            if (!focusField.absBound.equals(absBound)) {
                notifyVirtualViewExited(mContainerView, index);
                notifyVirtualViewEntered(mContainerView, index, absBound);
                // Update focus field position.
                mRequest.setFocusField(new FocusField(focusField.fieldIndex, absBound));
            }
        }
        notifyVirtualValueChanged(index, /* forceNotify= */ false);
        mAutofillUMA.onUserChangeFieldValue(mRequest.getField(sIndex).hasPreviouslyAutofilled());
    }

    /**
     * Invoked by the native counterpart when one or more fields have changed
     * their visibility. The (Java) fields' visibility state has at that point
     * already been updated by direct calls from native to the fields.
     *
     * @param indices the indices of the fields with visibility changes.
     *
     */
    @CalledByNative
    private void onFormFieldVisibilitiesDidChange(int[] indices) {
        if (mRequest == null || indices.length == 0) return;

        mAutofillUMA.onFieldChangedVisibility();

        for (int index : indices) {
            notifyVirtualViewVisibilityChanged(
                    index, mRequest.getField((short) index).getVisible());
        }
    }

    /**
     * Invoked when text field is scrolled.
     *
     * @param index index of field in current form.
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     *
     */
    @CalledByNative
    public void onTextFieldDidScroll(int index, float x, float y, float width, float height) {
        if (mRequest == null) return;

        short sIndex = (short) index;
        FormFieldData fieldData = mRequest.getField(sIndex);
        if (fieldData != null) fieldData.updateBounds(new RectF(x, y, x + width, y + height));

        // crbug.com/730764 - from P and above, Android framework listens to the onScrollChanged()
        // and repositions the autofill UI automatically.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) return;

        FocusField focusField = mRequest.getFocusField();
        if (focusField == null || sIndex != focusField.fieldIndex) return;

        Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
        // Notify the new position to the Android framework. Note that we do not call
        // notifyVirtualViewExited() here intentionally to avoid flickering.
        notifyVirtualViewEntered(mContainerView, index, absBound);

        // Update focus field position.
        mRequest.setFocusField(new FocusField(focusField.fieldIndex, absBound));
    }

    // Add a specific method in order to mock it in test.
    protected void initializeNativeAutofillProvider(WebContents webContents) {
        AutofillProviderJni.get().init(this, webContents);
    }

    private boolean isDatalistField(int index) {
        assert index <= Short.MAX_VALUE;
        FormFieldData field = mRequest.getField((short) index);
        return field.mControlType == FormFieldData.ControlType.DATALIST;
    }

    // `ValueChanged`, `ViewVisibilityChanged`, `ViewEntered`, and `ViewExited`
    // events are not communicated to the Android Autofill service if the focused element is a
    // datalist. This avoids UI conflicts between the datalist popup (shown by WebView) and the
    // Android Autofill UI (shown by Android).
    // On submit and on autofill a `ValueChanged` event is still sent to the Android Autofill
    // service.
    private void notifyVirtualValueChanged(int index, boolean forceNotify) {
        if (!forceNotify && isDatalistField(index)) return;
        AutofillValue autofillValue = mRequest.getFieldNewValue(index);
        if (autofillValue == null) return;
        mAutofillManager.notifyVirtualValueChanged(
                mContainerView, mRequest.getFieldVirtualId((short) index), autofillValue);
    }

    private void notifyVirtualViewVisibilityChanged(int index, boolean isVisible) {
        if (isDatalistField(index)) return;
        mAutofillManager.notifyVirtualViewVisibilityChanged(
                mContainerView, mRequest.getFieldVirtualId((short) index), isVisible);
    }

    @RequiresApi(VERSION_CODES.TIRAMISU)
    private boolean showAutofillDialog(View parent, int index) {
        // Refer to notifyVirtualValueChanged() for the reason of the datalist's special handling.
        if (isDatalistField(index)) return false;

        return mAutofillManager.showAutofillDialog(
                parent, mRequest.getFieldVirtualId((short) index));
    }

    private void notifyVirtualViewEntered(View parent, int index, Rect absBounds) {
        // Refer to notifyVirtualValueChanged() for the reason of the datalist's special handling.
        if (isDatalistField(index)) return;
        mAutofillManager.notifyVirtualViewEntered(
                parent, mRequest.getFieldVirtualId((short) index), absBounds);
    }

    private void notifyVirtualViewExited(View parent, int index) {
        // Refer to notifyVirtualValueChanged() for the reason of the datalist's special handling.
        if (isDatalistField(index)) return;
        mAutofillManager.notifyVirtualViewExited(parent, mRequest.getFieldVirtualId((short) index));
    }

    /**
     * Invoked when current form will be submitted.
     * @param submissionSource the submission source, could be any member defined in
     * SubmissionSource.java
     */
    @CalledByNative
    public void onFormSubmitted(int submissionSource) {
        // The changes could be missing, like those made by Javascript, we'd better to notify
        // AutofillManager current values. also see crbug.com/353001 and crbug.com/732856.
        forceNotifyFormValues();
        mAutofillManager.commit(submissionSource);
        mRequest = null;
        mAutofillUMA.onFormSubmitted(submissionSource);
    }

    /**
     * Invoked when focus field changed.
     *
     * @param focusOnForm whether focus is still on form.
     * @param focusField the index of field has focus
     * @param x the boundary of focus field.
     * @param y the boundary of focus field.
     * @param width the boundary of focus field.
     * @param height the boundary of focus field.
     */
    @CalledByNative
    public void onFocusChanged(
            boolean focusOnForm, int focusField, float x, float y, float width, float height) {
        onFocusChangedImpl(
                focusOnForm, focusField, x, y, width, height, /* causedByValueChange= */ false);
    }

    @CalledByNative
    public void hideDatalistPopup() {
        if (mDatalistPopup == null) return;

        mDatalistPopup.dismiss();
        mDatalistPopup = null;
        mDatalistSuggestions = null;
        if (mWebContentsAccessibility != null) {
            mWebContentsAccessibility.onAutofillPopupDismissed();
        }
    }

    private void notifyViewExitBeforeDestroyRequest() {
        if (mRequest == null) return;
        FocusField focusField = mRequest.getFocusField();
        if (focusField == null) return;
        notifyVirtualViewExited(mContainerView, focusField.fieldIndex);
        mRequest.setFocusField(null);
    }

    private void onFocusChangedImpl(
            boolean focusOnForm,
            int focusField,
            float x,
            float y,
            float width,
            float height,
            boolean causedByValueChange) {
        // Check focusField inside short value? FocusOnNonFormField is called after form
        // submitted.
        if (mRequest == null) return;
        FocusField prev = mRequest.getFocusField();
        if (focusOnForm) {
            Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
            if (prev != null && prev.fieldIndex == focusField && absBound.equals(prev.absBound)) {
                return;
            }

            // Notify focus changed.
            if (prev != null) {
                notifyVirtualViewExited(mContainerView, prev.fieldIndex);
            }

            notifyVirtualViewEntered(mContainerView, focusField, absBound);

            if (!causedByValueChange) {
                // The focus field value might not sync with platform's
                // AutofillManager, just notify it value changed.
                notifyVirtualValueChanged(focusField, /* forceNotify= */ false);
                mAutofillTriggeredTimeMillis = System.currentTimeMillis();
            }
            mRequest.setFocusField(new FocusField((short) focusField, absBound));
        } else {
            if (prev == null) return;
            // Notify focus changed.
            notifyVirtualViewExited(mContainerView, prev.fieldIndex);
            mRequest.setFocusField(null);
        }
    }

    @CalledByNative
    protected void showDatalistPopup(
            String[] datalistValues, String[] datalistLabels, boolean isRtl) {
        if (mRequest == null) return;
        FocusField focusField = mRequest.getFocusField();
        if (focusField != null) {
            showDatalistPopup(
                    datalistValues,
                    datalistLabels,
                    mRequest.getField(focusField.fieldIndex).getBounds(),
                    isRtl);
        }
    }

    /**
     * Display the simplest popup for the datalist. This is same as WebView's datalist popup in
     * Android pre-o. No suggestion from the autofill service will be presented, No advance
     * features of AutofillPopup are used.
     */
    private void showDatalistPopup(
            String[] datalistValues, String[] datalistLabels, RectF bounds, boolean isRtl) {
        mDatalistSuggestions = new AutofillSuggestion[datalistValues.length];
        for (int i = 0; i < mDatalistSuggestions.length; i++) {
            mDatalistSuggestions[i] =
                    new AutofillSuggestion.Builder()
                            .setLabel(datalistValues[i])
                            .setSubLabel(datalistLabels[i])
                            .setItemTag("")
                            .setSuggestionType(SuggestionType.DATALIST_ENTRY)
                            .setFeatureForIPH("")
                            .build();
        }
        if (mWebContentsAccessibility == null) {
            mWebContentsAccessibility = WebContentsAccessibility.fromWebContents(mWebContents);
        }
        if (mDatalistPopup == null) {
            if (ContextUtils.activityFromContext(mContext) == null) return;
            ViewAndroidDelegate delegate = mWebContents.getViewAndroidDelegate();
            if (mAnchorView == null) mAnchorView = delegate.acquireView();
            setAnchorViewRect(bounds);
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                mDatalistPopup =
                        new AutofillPopup(
                                mContext,
                                mAnchorView,
                                new AutofillDelegate() {
                                    @Override
                                    public void dismissed() {
                                        onDatalistPopupDismissed();
                                    }

                                    @Override
                                    public void suggestionSelected(int listIndex) {
                                        onSuggestionSelected(
                                                mDatalistSuggestions[listIndex].getLabel());
                                    }

                                    @Override
                                    public void deleteSuggestion(int listIndex) {}

                                    @Override
                                    public void accessibilityFocusCleared() {
                                        mWebContentsAccessibility
                                                .onAutofillPopupAccessibilityFocusCleared();
                                    }
                                },
                                null);
            } catch (RuntimeException e) {
                // Deliberately swallowing exception because bad framework implementation can
                // throw exceptions in ListPopupWindow constructor.
                onDatalistPopupDismissed();
                return;
            }
        }
        mDatalistPopup.filterAndShow(mDatalistSuggestions, isRtl);
        if (mWebContentsAccessibility != null) {
            mWebContentsAccessibility.onAutofillPopupDisplayed(mDatalistPopup.getListView());
        }
    }

    private void onDatalistPopupDismissed() {
        ViewAndroidDelegate delegate = mWebContents.getViewAndroidDelegate();
        if (delegate != null) delegate.removeView(mAnchorView);
        mAnchorView = null;
    }

    private void onSuggestionSelected(String value) {
        if (mNativeAutofillProvider != 0) {
            acceptDataListSuggestion(mNativeAutofillProvider, value);
        }
        hideDatalistPopup();
    }

    private void setAnchorViewRect(RectF rect) {
        if (mNativeAutofillProvider != 0) {
            setAnchorViewRect(mNativeAutofillProvider, mAnchorView, rect);
        }
    }

    @CalledByNative
    protected void setNativeAutofillProvider(long nativeAutofillProvider) {
        if (nativeAutofillProvider == mNativeAutofillProvider) return;
        // Setting the mNativeAutofillProvider to 0 may occur as a
        // result of WebView.destroy, or because a WebView has been
        // gc'ed. In the former case we can go ahead and clean up the
        // frameworks autofill manager, but in the latter case the
        // binder connection has already been dropped in a framework
        // finalizer, and so the methods we call will throw. It's not
        // possible to know which case we're in, so just catch the exception
        // in AutofillManagerWrapper.destroy().
        if (mNativeAutofillProvider != 0) mRequest = null;
        mNativeAutofillProvider = nativeAutofillProvider;
    }

    private void detachFromJavaAutofillProvider() {
        if (mNativeAutofillProvider == 0) return;
        // Makes sure this is the last call to mNativeAutofillProvider.
        long nativeAutofillProvider = mNativeAutofillProvider;
        mNativeAutofillProvider = 0;
        AutofillProviderJni.get().detachFromJavaAutofillProvider(nativeAutofillProvider);
    }

    public void setWebContents(WebContents webContents) {
        if (webContents == mWebContents) return;
        mAutofillUMA.recordSession();
        if (mWebContents != null) mRequest = null;
        mWebContents = webContents;
        detachFromJavaAutofillProvider();
        if (mWebContents != null) {
            initializeNativeAutofillProvider(webContents);
        }
    }

    @CalledByNative
    protected void onDidFillAutofillFormData() {
        // The changes were caused by the autofill service autofill form,
        // notified it about the result.
        forceNotifyFormValues();
    }

    @CalledByNative
    private void onServerPredictionsAvailable() {
        if (mRequest == null) return;
        mRequest.onServerPredictionsAvailable();
        mAutofillManager.onServerPredictionsAvailable();
        mAutofillUMA.onServerTypeAvailable(mRequest.getForm(), /* afterSessionStarted= */ true);
    }

    private void forceNotifyFormValues() {
        if (mRequest == null) return;
        for (int i = 0; i < mRequest.getFieldCount(); ++i) {
            notifyVirtualValueChanged(i, /* forceNotify= */ true);
        }
    }

    public AutofillPopup getDatalistPopupForTesting() {
        return mDatalistPopup;
    }

    private Rect transformToWindowBounds(RectF rect) {
        // Refer to crbug.com/1085294 for the reason of offset.
        // The current version of Mockito didn't support mock static method, adding extra method so
        // the transform can be tested.
        return transformToWindowBoundsWithOffsetY(
                rect, RenderCoordinates.fromWebContents(mWebContents).getContentOffsetYPixInt());
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public Rect transformToWindowBoundsWithOffsetY(RectF rect, int offsetY) {
        // Convert bounds to device pixel.
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        DisplayAndroid displayAndroid = windowAndroid.getDisplay();
        float dipScale = displayAndroid.getDipScale();
        RectF bounds = new RectF(rect);
        Matrix matrix = new Matrix();
        matrix.setScale(dipScale, dipScale);
        int[] location = new int[2];
        mContainerView.getLocationOnScreen(location);
        location[1] += offsetY;
        matrix.postTranslate(location[0], location[1]);
        matrix.mapRect(bounds);
        return new Rect(
                (int) bounds.left, (int) bounds.top, (int) bounds.right, (int) bounds.bottom);
    }

    /**
     * Transform FormFieldData's bounds to ContainView's coordinates and update the bounds with the
     * transformed one.
     *
     * @param formData the form need to be transformed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public void transformFormFieldToContainViewCoordinates(FormData formData) {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        DisplayAndroid displayAndroid = windowAndroid.getDisplay();
        float dipScale = displayAndroid.getDipScale();
        Matrix matrix = new Matrix();
        matrix.setScale(dipScale, dipScale);
        matrix.postTranslate(mContainerView.getScrollX(), mContainerView.getScrollY());

        for (FormFieldData field : formData.mFields) {
            RectF bounds = new RectF();
            matrix.mapRect(bounds, field.getBounds());
            field.setBoundsInContainerViewCoordinates(bounds);
        }
    }

    /**
     * Inform native provider to autofill.
     *
     * @param nativeAutofillProvider the native autofill provider.
     */
    private void autofill(long nativeAutofillProvider) {
        AutofillProviderJni.get().onAutofillAvailable(nativeAutofillProvider);
    }

    private void acceptDataListSuggestion(long nativeAutofillProvider, String value) {
        AutofillProviderJni.get().onAcceptDataListSuggestion(nativeAutofillProvider, value);
    }

    private void setAnchorViewRect(long nativeAutofillProvider, View anchorView, RectF rect) {
        AutofillProviderJni.get()
                .setAnchorViewRect(
                        nativeAutofillProvider,
                        anchorView,
                        rect.left,
                        rect.top,
                        rect.width(),
                        rect.height());
    }

    @CalledByNative
    public void cancelSession() {
        mAutofillManager.cancel();
        mPrefillRequest = null;
        mRequest = null;
    }

    @CalledByNative
    public void reset() {
        hideDatalistPopup();
        mPrefillRequest = null;
        mRequest = null;
    }

    @NativeMethods
    interface Natives {
        void init(AutofillProvider caller, WebContents webContents);

        void detachFromJavaAutofillProvider(long nativeAndroidAutofillProviderBridgeImpl);

        void onAutofillAvailable(long nativeAndroidAutofillProviderBridgeImpl);

        void onAcceptDataListSuggestion(
                long nativeAndroidAutofillProviderBridgeImpl,
                @JniType("std::u16string") String value);

        void setAnchorViewRect(
                long nativeAndroidAutofillProviderBridgeImpl,
                View anchorView,
                float x,
                float y,
                float width,
                float height);

        void onShowBottomSheetResult(
                long nativeAndroidAutofillProviderBridgeImpl,
                boolean isShown,
                boolean providedAutofillStructure);
    }
}
