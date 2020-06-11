// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStructure;
import android.view.autofill.AutofillValue;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.DoNotInline;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.components.version_info.VersionConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

/**
 * This class uses Android autofill service to fill web form. All methods are
 * supposed to be called in UI thread.
 *
 * This class doesn't have 1:1 mapping to native AutofillProviderAndroid; the
 * normal ownership model is that this object is owned by the embedder-specific
 * Java WebContents wrapper (e.g., AwContents.java in //android_webview), and
 * AutofillProviderAndroid is owned by the embedder-specific C++ WebContents
 * wrapper (e.g., native AwContents in //android_webview).
 *
 * DoNotInline since it causes class verification errors, see crbug.com/991851.
 */
@DoNotInline
@TargetApi(Build.VERSION_CODES.O)
public class AutofillProviderImpl extends AutofillProvider {
    private static final String TAG = "AutofillProviderImpl";
    private static class FocusField {
        public final short fieldIndex;
        public final Rect absBound;

        public FocusField(short fieldIndex, Rect absBound) {
            this.fieldIndex = fieldIndex;
            this.absBound = absBound;
        }
    }
    /**
     * The class to wrap the request to framework.
     *
     * Though framework guarantees always giving us the autofill value of current
     * session, we still want to verify this by using unique virtual id which is
     * composed of sessionId and form field index, we don't use the request id
     * which comes from renderer as session id because it is not unique.
     */
    private static class AutofillRequest {
        private static final int INIT_ID = 1; // ID can't be 0 in Android.
        private static int sSessionId = INIT_ID;
        public final int sessionId;
        private FormData mFormData;
        private FocusField mFocusField;

        public AutofillRequest(FormData formData, FocusField focus) {
            sessionId = getNextClientId();
            mFormData = formData;
            mFocusField = focus;
        }

        public void fillViewStructure(ViewStructure structure) {
            structure.setWebDomain(mFormData.mHost);
            structure.setHtmlInfo(structure.newHtmlInfoBuilder("form")
                                          .addAttribute("name", mFormData.mName)
                                          .build());
            int index = structure.addChildCount(mFormData.mFields.size());
            short fieldIndex = 0;
            for (FormFieldData field : mFormData.mFields) {
                ViewStructure child = structure.newChild(index++);
                int virtualId = toVirtualId(sessionId, fieldIndex++);
                child.setAutofillId(structure.getAutofillId(), virtualId);
                if (field.mAutocompleteAttr != null && !field.mAutocompleteAttr.isEmpty()) {
                    child.setAutofillHints(field.mAutocompleteAttr.split(" +"));
                }
                child.setHint(field.mPlaceholder);

                RectF bounds = field.getBoundsInContainerViewCoordinates();
                // Field has no scroll.
                child.setDimens((int) bounds.left, (int) bounds.top, 0 /* scrollX*/,
                        0 /* scrollY */, (int) bounds.width(), (int) bounds.height());

                ViewStructure.HtmlInfo.Builder builder =
                        child.newHtmlInfoBuilder("input")
                                .addAttribute("name", field.mName)
                                .addAttribute("type", field.mType)
                                .addAttribute("label", field.mLabel)
                                .addAttribute("ua-autofill-hints", field.mHeuristicType)
                                .addAttribute("id", field.mId);

                switch (field.getControlType()) {
                    case FormFieldData.ControlType.LIST:
                        child.setAutofillType(View.AUTOFILL_TYPE_LIST);
                        child.setAutofillOptions(field.mOptionContents);
                        int i = findIndex(field.mOptionValues, field.getValue());
                        if (i != -1) {
                            child.setAutofillValue(AutofillValue.forList(i));
                        }
                        break;
                    case FormFieldData.ControlType.TOGGLE:
                        child.setAutofillType(View.AUTOFILL_TYPE_TOGGLE);
                        child.setAutofillValue(AutofillValue.forToggle(field.isChecked()));
                        break;
                    case FormFieldData.ControlType.TEXT:
                    case FormFieldData.ControlType.DATALIST:
                        child.setAutofillType(View.AUTOFILL_TYPE_TEXT);
                        child.setAutofillValue(AutofillValue.forText(field.getValue()));
                        if (field.mMaxLength != 0) {
                            builder.addAttribute("maxlength", String.valueOf(field.mMaxLength));
                        }
                        if (field.getControlType() == FormFieldData.ControlType.DATALIST) {
                            child.setAutofillOptions(field.mDatalistValues);
                        }
                        break;
                    default:
                        break;
                }
                child.setHtmlInfo(builder.build());
            }
        }

        public boolean autofill(final SparseArray<AutofillValue> values) {
            for (int i = 0; i < values.size(); ++i) {
                int id = values.keyAt(i);
                if (toSessionId(id) != sessionId) return false;
                AutofillValue value = values.get(id);
                if (value == null) continue;
                short index = toIndex(id);
                if (index < 0 || index >= mFormData.mFields.size()) return false;
                FormFieldData field = mFormData.mFields.get(index);
                if (field == null) return false;
                try {
                    switch (field.getControlType()) {
                        case FormFieldData.ControlType.LIST:
                            int j = value.getListValue();
                            if (j < 0 && j >= field.mOptionValues.length) continue;
                            field.setAutofillValue(field.mOptionValues[j]);
                            break;
                        case FormFieldData.ControlType.TOGGLE:
                            field.setChecked(value.getToggleValue());
                            break;
                        case FormFieldData.ControlType.TEXT:
                        case FormFieldData.ControlType.DATALIST:
                            field.setAutofillValue((String) value.getTextValue());
                            break;
                        default:
                            break;
                    }
                } catch (IllegalStateException e) {
                    // Refer to crbug.com/1080580 .
                    Log.e(TAG, "The given AutofillValue wasn't expected, abort autofill.", e);
                    return false;
                }
            }
            return true;
        }

        public void setFocusField(FocusField focusField) {
            mFocusField = focusField;
        }

        public FocusField getFocusField() {
            return mFocusField;
        }

        public int getFieldCount() {
            return mFormData.mFields.size();
        }

        public AutofillValue getFieldNewValue(int index) {
            FormFieldData field = mFormData.mFields.get(index);
            if (field == null) return null;
            switch (field.getControlType()) {
                case FormFieldData.ControlType.LIST:
                    int i = findIndex(field.mOptionValues, field.getValue());
                    if (i == -1) return null;
                    return AutofillValue.forList(i);
                case FormFieldData.ControlType.TOGGLE:
                    return AutofillValue.forToggle(field.isChecked());
                case FormFieldData.ControlType.TEXT:
                case FormFieldData.ControlType.DATALIST:
                    return AutofillValue.forText(field.getValue());
                default:
                    return null;
            }
        }

        public int getVirtualId(short index) {
            return toVirtualId(sessionId, index);
        }

        public FormFieldData getField(short index) {
            return mFormData.mFields.get(index);
        }

        private static int findIndex(String[] values, String value) {
            if (values != null && value != null) {
                for (int i = 0; i < values.length; i++) {
                    if (value.equals(values[i])) return i;
                }
            }
            return -1;
        }

        private static int getNextClientId() {
            ThreadUtils.assertOnUiThread();
            if (sSessionId == 0xffff) sSessionId = INIT_ID;
            return sSessionId++;
        }

        private static int toSessionId(int virtualId) {
            return (virtualId & 0xffff0000) >> 16;
        }

        private static short toIndex(int virtualId) {
            return (short) (virtualId & 0xffff);
        }

        private static int toVirtualId(int clientId, short index) {
            return (clientId << 16) | index;
        }
    }

    private final String mProviderName;
    private AutofillManagerWrapper mAutofillManager;
    private ViewGroup mContainerView;
    private WebContents mWebContents;

    private AutofillRequest mRequest;
    private long mNativeAutofillProvider;
    private AutofillProviderUMA mAutofillUMA;
    private AutofillManagerWrapper.InputUIObserver mInputUIObserver;
    private long mAutofillTriggeredTimeMillis;

    public AutofillProviderImpl(Context context, ViewGroup containerView, String providerName) {
        this(containerView, new AutofillManagerWrapper(context), context, providerName);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public AutofillProviderImpl(ViewGroup containerView, AutofillManagerWrapper manager,
            Context context, String providerName) {
        mProviderName = providerName;
        try (ScopedSysTraceEvent e =
                        ScopedSysTraceEvent.scoped("AutofillProviderImpl.constructor")) {
            assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
            mAutofillManager = manager;
            mContainerView = containerView;
            mAutofillUMA = new AutofillProviderUMA(context);
            mInputUIObserver = new AutofillManagerWrapper.InputUIObserver() {
                @Override
                public void onInputUIShown() {
                    // Not need to report suggestion window displayed if there is no live autofill
                    // session.
                    if (mRequest == null) return;
                    mAutofillUMA.onSuggestionDisplayed(
                            System.currentTimeMillis() - mAutofillTriggeredTimeMillis);
                }
            };
            mAutofillManager.addInputUIObserver(mInputUIObserver);
        }
    }

    @Override
    public void onContainerViewChanged(ViewGroup containerView) {
        mContainerView = containerView;
    }

    @Override
    public void onProvideAutoFillVirtualStructure(ViewStructure structure, int flags) {
        // This method could be called for the session started by the native
        // control outside of the scope of autofill, e.g. the URL bar, in this case, we simply
        // return.
        if (mRequest == null) return;

        Bundle bundle = structure.getExtras();
        if (bundle != null) {
            bundle.putCharSequence("VIRTUAL_STRUCTURE_PROVIDER_NAME", mProviderName);
            bundle.putCharSequence(
                    "VIRTUAL_STRUCTURE_PROVIDER_VERSION", VersionConstants.PRODUCT_VERSION);
        }
        mRequest.fillViewStructure(structure);
        if (AutofillManagerWrapper.isLoggable()) {
            AutofillManagerWrapper.log(
                    "onProvideAutoFillVirtualStructure fields:" + structure.getChildCount());
        }
        mAutofillUMA.onVirtualStructureProvided();
    }

    @Override
    public void autofill(final SparseArray<AutofillValue> values) {
        if (mNativeAutofillProvider != 0 && mRequest != null && mRequest.autofill((values))) {
            autofill(mNativeAutofillProvider, mRequest.mFormData);
            if (AutofillManagerWrapper.isLoggable()) {
                AutofillManagerWrapper.log("autofill values:" + values.size());
            }
            mAutofillUMA.onAutofill();
        }
    }

    @Override
    public boolean shouldQueryAutofillSuggestion() {
        return mRequest != null && mRequest.getFocusField() != null
                && !mAutofillManager.isAutofillInputUIShowing();
    }

    @Override
    public void queryAutofillSuggestion() {
        if (shouldQueryAutofillSuggestion()) {
            FocusField focusField = mRequest.getFocusField();
            mAutofillManager.requestAutofill(mContainerView,
                    mRequest.getVirtualId(focusField.fieldIndex), focusField.absBound);
        }
    }

    @Override
    public void startAutofillSession(
            FormData formData, int focus, float x, float y, float width, float height) {
        // Check focusField inside short value?
        // Autofill Manager might have session that wasn't started by AutofillProviderImpl,
        // we just always cancel existing session here.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            mAutofillManager.cancel();
        }

        Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
        if (mRequest != null) notifyViewExitBeforeDestroyRequest();
        transformFormFieldToContainViewCoordinates(formData);
        mRequest = new AutofillRequest(formData, new FocusField((short) focus, absBound));
        int virtualId = mRequest.getVirtualId((short) focus);
        notifyVirtualViewEntered(mContainerView, virtualId, absBound);
        mAutofillUMA.onSessionStarted(mAutofillManager.isDisabled());
        mAutofillTriggeredTimeMillis = System.currentTimeMillis();

        mAutofillManager.notifyNewSessionStarted();
    }

    @Override
    public void onFormFieldDidChange(int index, float x, float y, float width, float height) {
        // Check index inside short value?
        if (mRequest == null) return;

        short sIndex = (short) index;
        FocusField focusField = mRequest.getFocusField();
        if (focusField == null || sIndex != focusField.fieldIndex) {
            onFocusChangedImpl(true, index, x, y, width, height, true /*causedByValueChange*/);
        } else {
            // Currently there is no api to notify both value and position
            // change, before the API is available, we still need to call
            // notifyVirtualViewEntered() to tell current coordinates because
            // the position could be changed.
            int virtualId = mRequest.getVirtualId(sIndex);
            Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
            if (!focusField.absBound.equals(absBound)) {
                notifyVirtualViewExited(mContainerView, virtualId);
                notifyVirtualViewEntered(mContainerView, virtualId, absBound);
                // Update focus field position.
                mRequest.setFocusField(new FocusField(focusField.fieldIndex, absBound));
            }
        }
        notifyVirtualValueChanged(index, /* forceNotify = */ false);
        mAutofillUMA.onUserChangeFieldValue(mRequest.getField(sIndex).hasPreviouslyAutofilled());
    }

    @Override
    public void onTextFieldDidScroll(int index, float x, float y, float width, float height) {
        // crbug.com/730764 - from P and above, Android framework listens to the onScrollChanged()
        // and repositions the autofill UI automatically.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) return;
        if (mRequest == null) return;

        short sIndex = (short) index;
        FocusField focusField = mRequest.getFocusField();
        if (focusField == null || sIndex != focusField.fieldIndex) return;

        int virtualId = mRequest.getVirtualId(sIndex);
        Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
        // Notify the new position to the Android framework. Note that we do not call
        // notifyVirtualViewExited() here intentionally to avoid flickering.
        notifyVirtualViewEntered(mContainerView, virtualId, absBound);

        // Update focus field position.
        mRequest.setFocusField(new FocusField(focusField.fieldIndex, absBound));
    }

    private boolean isDatalistField(int childId) {
        FormFieldData field = mRequest.getField((short) childId);
        return field.mControlType == FormFieldData.ControlType.DATALIST;
    }

    private void notifyVirtualValueChanged(int index, boolean forceNotify) {
        // The ValueChanged, ViewEntered and ViewExited aren't notified to the autofill service for
        // the focused datalist to avoid the potential UI conflict.
        // The datalist support was added later and the option list is displayed by WebView, the
        // autofill service might also show its suggestions when the datalist (associated the input
        // field) is focused, the two UI overlap, the solution is to completely hide the fact that
        // the datalist is being focused to the autofill service to prevent it from displaying the
        // suggestion.
        // The ValueChange will still be sent to autofill service when the form
        // submitted or autofilled.
        if (!forceNotify && isDatalistField(index)) return;
        AutofillValue autofillValue = mRequest.getFieldNewValue(index);
        if (autofillValue == null) return;
        mAutofillManager.notifyVirtualValueChanged(
                mContainerView, mRequest.getVirtualId((short) index), autofillValue);
    }

    private void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
        // Refer to notifyVirtualValueChanged() for the reason of the datalist's special handling.
        if (isDatalistField(childId)) return;
        mAutofillManager.notifyVirtualViewEntered(parent, childId, absBounds);
    }

    private void notifyVirtualViewExited(View parent, int childId) {
        // Refer to notifyVirtualValueChanged() for the reason of the datalist's special handling.
        if (isDatalistField(childId)) return;
        mAutofillManager.notifyVirtualViewExited(parent, childId);
    }
    @Override
    public void onFormSubmitted(int submissionSource) {
        // The changes could be missing, like those made by Javascript, we'd better to notify
        // AutofillManager current values. also see crbug.com/353001 and crbug.com/732856.
        forceNotifyFormValues();
        mAutofillManager.commit(submissionSource);
        mRequest = null;
        mAutofillUMA.onFormSubmitted(submissionSource);
    }

    @Override
    public void onFocusChanged(
            boolean focusOnForm, int focusField, float x, float y, float width, float height) {
        onFocusChangedImpl(
                focusOnForm, focusField, x, y, width, height, false /*causedByValueChange*/);
    }

    @Override
    protected void hidePopup() {}

    private void notifyViewExitBeforeDestroyRequest() {
        if (mRequest == null) return;
        FocusField focusField = mRequest.getFocusField();
        if (focusField == null) return;
        notifyVirtualViewExited(mContainerView, mRequest.getVirtualId(focusField.fieldIndex));
        mRequest.setFocusField(null);
    }

    private void onFocusChangedImpl(boolean focusOnForm, int focusField, float x, float y,
            float width, float height, boolean causedByValueChange) {
        // Check focusField inside short value?
        // FocusNoLongerOnForm is called after form submitted.
        if (mRequest == null) return;
        FocusField prev = mRequest.getFocusField();
        if (focusOnForm) {
            Rect absBound = transformToWindowBounds(new RectF(x, y, x + width, y + height));
            if (prev != null && prev.fieldIndex == focusField && absBound.equals(prev.absBound)) {
                return;
            }

            // Notify focus changed.
            if (prev != null) {
                notifyVirtualViewExited(mContainerView, mRequest.getVirtualId(prev.fieldIndex));
            }

            notifyVirtualViewEntered(
                    mContainerView, mRequest.getVirtualId((short) focusField), absBound);

            if (!causedByValueChange) {
                // The focus field value might not sync with platform's
                // AutofillManager, just notify it value changed.
                notifyVirtualValueChanged(focusField, /* forceNotify = */ false);
                mAutofillTriggeredTimeMillis = System.currentTimeMillis();
            }
            mRequest.setFocusField(new FocusField((short) focusField, absBound));
        } else {
            if (prev == null) return;
            // Notify focus changed.
            notifyVirtualViewExited(mContainerView, mRequest.getVirtualId(prev.fieldIndex));
            mRequest.setFocusField(null);
        }
    }

    @Override
    protected void reset() {
        // We don't need to reset anything here, it should be safe to cancel
        // current autofill session when new one starts in
        // startAutofillSession().
    }

    @Override
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
        if (nativeAutofillProvider == 0) mAutofillManager.destroy();
    }

    @Override
    public void setWebContents(WebContents webContents) {
        if (webContents == mWebContents) return;
        if (mWebContents != null) mRequest = null;
        mWebContents = webContents;
    }

    @Override
    protected void onDidFillAutofillFormData() {
        // The changes were caused by the autofill service autofill form,
        // notified it about the result.
        forceNotifyFormValues();
    }

    private void forceNotifyFormValues() {
        if (mRequest == null) return;
        for (int i = 0; i < mRequest.getFieldCount(); ++i) {
            notifyVirtualValueChanged(i, /* forceNotify = */ true);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public Rect transformToWindowBounds(RectF rect) {
        // Convert bounds to device pixel.
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        DisplayAndroid displayAndroid = windowAndroid.getDisplay();
        float dipScale = displayAndroid.getDipScale();
        RectF bounds = new RectF(rect);
        Matrix matrix = new Matrix();
        matrix.setScale(dipScale, dipScale);
        int[] location = new int[2];
        mContainerView.getLocationOnScreen(location);
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
}
