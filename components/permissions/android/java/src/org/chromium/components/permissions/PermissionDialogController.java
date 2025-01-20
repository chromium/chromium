// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.Button;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.util.DimensionCompat;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedList;
import java.util.List;

/**
 * Singleton instance which controls the display of modal permission dialogs. This class is lazily
 * initiated when getInstance() is first called.
 *
 * Unlike permission infobars, which stack on top of each other, only one permission dialog may be
 * visible on the screen at once. Any additional request for a modal permissions dialog is queued,
 * and will be displayed once the user responds to the current dialog.
 */
public class PermissionDialogController
        implements AndroidPermissionRequester.RequestDelegate, ModalDialogProperties.Controller {
    @IntDef({
        State.NOT_SHOWING,
        State.PROMPT_OPEN,
        State.PROMPT_POSITIVE_CLICKED,
        State.PROMPT_NEGATIVE_CLICKED,
        State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT,
        State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT,
        State.PROMPT_POSITIVE_EPHEMERAL_CLICKED
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        int NOT_SHOWING = 0;
        // We don't show prompts while Chrome Home is showing.
        // int PROMPT_PENDING = 1; // Obsolete.
        int PROMPT_OPEN = 2;
        int PROMPT_POSITIVE_CLICKED = 3;
        int PROMPT_NEGATIVE_CLICKED = 4;
        int REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT = 5;
        int REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT = 6;
        int PROMPT_POSITIVE_EPHEMERAL_CLICKED = 7;
    }

    /** Interface for a class that wants to receive updates from this controller. */
    public interface Observer {
        /**
         * Notifies the observer that the user has just completed a permissions prompt.
         *
         * @param window The {@link WindowAndroid} for the prompt that just finished.
         * @param permissions An array of ContentSettingsType, indicating the last dialog
         *     permissions.
         * @param result A ContentSettingValues type, indicating the last dialog result.
         */
        void onDialogResult(
                WindowAndroid window,
                @ContentSettingsType.EnumType int[] permissions,
                @ContentSettingValues int result);
    }

    private final ObserverList<Observer> mObservers;

    private PropertyModel mDialogModel;
    private PropertyModel mCustomViewModel;
    private PropertyModelChangeProcessor mCustomViewModelChangeProcessor;
    private PropertyModel mOverlayDetectedDialogModel;
    private PermissionDialogDelegate mDialogDelegate;
    private ModalDialogManager mModalDialogManager;
    private ModalDialogManagerObserver mModalDialogManagerObserver;

    // As the PermissionRequestManager handles queueing for a tab and only shows prompts for active
    // tabs, we typically only have one request. This class only handles multiple requests at once
    // when either:
    // 1) Multiple open windows request permissions due to Android split-screen
    // 2) A tab navigates or is closed while the Android permission request is open, and the
    // subsequent page requests a permission
    private List<PermissionDialogDelegate> mRequestQueue;

    /** The current state, whether we have a prompt showing and so on. */
    private @State int mState;

    // Static holder to ensure safe initialization of the singleton instance.
    private static class Holder {
        @SuppressLint("StaticFieldLeak")
        private static final PermissionDialogController sInstance =
                new PermissionDialogController();
    }

    public static PermissionDialogController getInstance() {
        return Holder.sInstance;
    }

    private PermissionDialogController() {
        mRequestQueue = new LinkedList<>();
        mState = State.NOT_SHOWING;
        mObservers = new ObserverList<>();
    }

    /**
     * Called by native code to create a modal permission dialog. The PermissionDialogController
     * will decide whether the dialog needs to be queued (because another dialog is on the screen)
     * or whether it can be shown immediately.
     */
    @CalledByNative
    private static void createDialog(PermissionDialogDelegate delegate) {
        PermissionDialogController.getInstance().queueDialog(delegate);
    }

    /**
     * @param observer An observer to be notified of changes.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** @param observer The observer to remove. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Update the custom view for the given native delegate, if the delegate points to the current
     * displaying dialog.
     *
     * @param delegate The wrapper for the native-side permission delegate.
     */
    public void updateCustomView(PermissionDialogDelegate delegate) {
        if (mDialogDelegate != delegate) {
            return;
        }
        // TODO(crbug.com/388407665): Setup and bind layout based on the variant
        mDialogModel.set(ModalDialogProperties.CUSTOM_VIEW, setupCustomView());
    }

    /**
     * Queues a modal permission dialog for display. If there are currently no dialogs on screen, it
     * will be displayed immediately. Otherwise, it will be displayed as soon as the user responds
     * to the current dialog.
     *
     * @param context The context to use to get the dialog layout.
     * @param delegate The wrapper for the native-side permission delegate.
     */
    private void queueDialog(PermissionDialogDelegate delegate) {
        mRequestQueue.add(delegate);
        delegate.setDialogController(this);
        scheduleDisplay();
    }

    private void scheduleDisplay() {
        if (mState == State.NOT_SHOWING && !mRequestQueue.isEmpty()) dequeueDialog();
    }

    @Override
    public void onAndroidPermissionAccepted() {
        assert mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT
                || mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT;

        // The tab may have navigated or been closed behind the Android permission prompt.
        if (mDialogDelegate == null) {
            mState = State.NOT_SHOWING;
        } else {
            // If this dialog comes from a embedded permission request, it might have multiple of
            // different screens showing up. It makes more sense to notify the observers (in this
            // case to change the icon) after all the screens have appeared.
            if (!isEmbeddedPrompt()) {
                notifyObservers(ContentSettingValues.ALLOW);
            }
            if (mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT) {
                mDialogDelegate.onAccept();
            } else {
                // State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT
                mDialogDelegate.onAcceptThisTime();
            }
        }
        scheduleDisplay();
    }

    @Override
    public void onAndroidPermissionCanceled() {
        assert mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT
                || mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT;

        // The tab may have navigated or been closed behind the Android permission prompt.
        if (mDialogDelegate == null) {
            mState = State.NOT_SHOWING;
        } else {
            notifyObservers(ContentSettingValues.DEFAULT);
            // The user accepted the site-level prompt but denied the app-level prompt.
            // No content setting should be set.
            mDialogDelegate.onDismiss(DismissalType.AUTODISMISS_OS_DENIED);
        }
        scheduleDisplay();
    }

    /**
     * Setup a custom view for permission dialog, also set up the MVC model for the dialog's custom
     * view.
     */
    private View setupCustomView() {
        Context context = getContext();
        // One time prompts don't share the same layout.
        View customView =
                mDialogDelegate.canShowEphemeralOption()
                        ? LayoutInflaterUtils.inflate(
                                context, R.layout.permission_dialog_one_time_permission, null)
                        : LayoutInflaterUtils.inflate(context, R.layout.permission_dialog, null);

        mCustomViewModel = PermissionDialogCustomViewModelFactory.getModel(mDialogDelegate);
        mCustomViewModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mCustomViewModel,
                        customView,
                        mDialogDelegate.canShowEphemeralOption()
                                ? PermissionOneTimeDialogCustomViewBinder::bind
                                : PermissionDialogCustomViewBinder::bind);
        return customView;
    }

    /** Shows the dialog asking the user for a web API permission. */
    public void dequeueDialog() {
        assert mState == State.NOT_SHOWING;

        mDialogDelegate = mRequestQueue.remove(0);
        Context context = getContext();

        // It's possible for the context to be null if we reach here just after the user
        // backgrounds the browser and cleanup has happened. In that case, we can't show a prompt,
        // so act as though the user dismissed it.
        if (context == null) {
            notifyObservers(ContentSettingValues.DEFAULT);
            // TODO(timloh): This probably doesn't work, as this happens synchronously when creating
            // the PermissionPromptAndroid, so the PermissionRequestManager won't be ready yet.
            mDialogDelegate.onDismiss(DismissalType.AUTODISMISS_NO_CONTEXT);
            return;
        }

        mModalDialogManager = mDialogDelegate.getWindow().getModalDialogManager();

        // The tab may have navigated or been closed while we were waiting for Chrome Home to close.
        // For some embedders (e.g. WebEngine) the layout might not be inflated and so the
        // ModalDialogManager is not available.
        if (mDialogDelegate == null || mModalDialogManager == null) {
            mState = State.NOT_SHOWING;
            scheduleDisplay();
            return;
        }

        // Setting up the Permission's dialog.
        mDialogModel =
                PermissionDialogModelFactory.getModel(
                        this,
                        mDialogDelegate,
                        setupCustomView(),
                        () -> showFilteredTouchEventDialog(context));

        mModalDialogManagerObserver =
                new ModalDialogManagerObserver() {
                    @Override
                    public void onDialogShown(View dialogView) {
                        // If the negative button is shown, it will be the last button.
                        ViewGroup buttonGroup = dialogView.findViewById(R.id.button_group);
                        if (buttonGroup == null || buttonGroup.getChildCount() == 0) {
                            return;
                        }
                        View lastView = buttonGroup.getChildAt(buttonGroup.getChildCount() - 1);
                        assert lastView instanceof Button;
                        Button lastButton = (Button) lastView;
                        if (lastButton.getVisibility() == View.GONE
                                || lastButton.getText()
                                        != mDialogDelegate.getNegativeButtonText()) {
                            return;
                        }

                        // When `showDialog` is called and triggered this callback, the button
                        // visibility is VISIBLE but the button might not been laid out yet. If that
                        // is the case, we try again after the next layout.
                        if (lastButton.isLaidOut()) {
                            recordOutOfScreenNegativeButton(lastButton);
                            return;
                        }
                        lastButton
                                .getViewTreeObserver()
                                .addOnGlobalLayoutListener(
                                        new ViewTreeObserver.OnGlobalLayoutListener() {
                                            @Override
                                            public void onGlobalLayout() {
                                                if (!lastButton.isLaidOut()) {
                                                    return;
                                                }
                                                lastButton
                                                        .getViewTreeObserver()
                                                        .removeOnGlobalLayoutListener(this);
                                                recordOutOfScreenNegativeButton(lastButton);
                                            }
                                        });
                    }
                };
        mModalDialogManager.addObserver(mModalDialogManagerObserver);
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
        mState = State.PROMPT_OPEN;
    }

    /** Record histogram if a button is rendered out of screen. */
    private void recordOutOfScreenNegativeButton(Button button) {
        int[] loc = new int[2];
        button.getLocationOnScreen(loc);
        int x = loc[0];
        int y = loc[1];
        DimensionCompat dimension =
                DimensionCompat.create(ContextUtils.activityFromContext(button.getContext()), null);

        boolean outOfScreen =
                x < 0 || y < 0 || x > dimension.getWindowWidth() || y > dimension.getWindowHeight();
        RecordHistogram.recordBooleanHistogram(
                "Permissions.OneTimePermission.Android.NegativeButtonOutOfScreen", outOfScreen);
    }

    /**
     * Displays the dialog explaining that Chrome has detected an overlay. Offers the user to close
     * the overlay window and try again.
     */
    private void showFilteredTouchEventDialog(Context context) {
        // Don't show another dialog if one is already displayed.
        if (mOverlayDetectedDialogModel != null) return;

        ModalDialogProperties.Controller overlayDetectedDialogController =
                new SimpleModalDialogController(
                        mModalDialogManager,
                        (Integer dismissalCause) -> {
                            if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                    && mDialogModel != null) {
                                mModalDialogManager.dismissDialog(
                                        mDialogModel,
                                        DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
                            }
                            mOverlayDetectedDialogModel = null;
                        });
        mOverlayDetectedDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, overlayDetectedDialogController)
                        .with(
                                ModalDialogProperties.TITLE,
                                context.getString(
                                        R.string.overlay_detected_dialog_title,
                                        BuildInfo.getInstance().hostPackageLabel))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                context.getString(R.string.overlay_detected_dialog_message))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getResources(),
                                R.string.cancel)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getResources(),
                                R.string.try_again)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();
        mModalDialogManager.showDialog(
                mOverlayDetectedDialogModel, ModalDialogManager.ModalDialogType.APP, true);
    }

    public void dismissFromNative(PermissionDialogDelegate delegate) {
        if (mDialogDelegate != delegate) {
            assert mRequestQueue.contains(delegate);
            mRequestQueue.remove(delegate);
        } else {
            boolean isEmbeddedPrompt = isEmbeddedPrompt();
            mDialogDelegate = null;
            if (isEmbeddedPrompt) {
                mModalDialogManager.dismissDialog(
                        mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
            } else {
                // Some caution is required here to handle cases where the user actions or dismisses
                // the prompt at roughly the same time as native. Due to asynchronicity, this
                // function may be called after onClick and before onDismiss, or before both of
                // those listeners.
                if (mState == State.PROMPT_OPEN) {
                    mModalDialogManager.dismissDialog(
                            mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
                } else {
                    assert mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT
                            || mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT
                            || mState == State.PROMPT_NEGATIVE_CLICKED
                            || mState == State.PROMPT_POSITIVE_CLICKED;
                }
            }
        }
        delegate.destroy();
        mState = State.NOT_SHOWING;
    }

    public void updateIcon(Bitmap icon) {
        if (mCustomViewModel == null) {
            return;
        }

        mCustomViewModel.set(
                PermissionDialogCustomViewProperties.ICON,
                new BitmapDrawable(getContext().getResources(), icon));
        mCustomViewModel.set(PermissionDialogCustomViewProperties.ICON_TINT, null);
    }

    public int getIconSizeInPx() {
        return getContext().getResources().getDimensionPixelSize(R.dimen.large_favicon_size);
    }

    private Context getContext() {
        assert mDialogDelegate != null;
        // Use the context to access resources instead of the activity because the activity may not
        // have the correct resources in some cases (e.g. WebLayer).
        return mDialogDelegate.getWindow().getContext().get();
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        // Called when the dialog is dismissed. Interacting with any button in the dialog will
        // call this handler after its own handler.
        // When the dialog is dismissed, the delegate's native pointers are
        // freed, and the next queued dialog (if any) is displayed.
        mDialogModel = null;
        if (mModalDialogManagerObserver != null) {
            mModalDialogManager.removeObserver(mModalDialogManagerObserver);
            mModalDialogManagerObserver = null;
        }
        mCustomViewModel = null;
        mCustomViewModelChangeProcessor.destroy();
        if (mDialogDelegate == null) {
            // We get into here if a tab navigates or is closed underneath the
            // prompt.
            mState = State.NOT_SHOWING;
            return;
        }

        if (mState == State.PROMPT_POSITIVE_EPHEMERAL_CLICKED) {
            requestAndroidPermissionsIfNecessary();
        } else if (mState == State.PROMPT_POSITIVE_CLICKED) {
            // Request android permission in embedded prompt in `PROMPT_POSITIVE_CLICKED` state will
            // be handled in onClick.
            if (!isEmbeddedPrompt()) {
                requestAndroidPermissionsIfNecessary();
            }
        } else if (mState == State.PROMPT_NEGATIVE_CLICKED) {
            if (!isEmbeddedPrompt()
                    || mDialogDelegate.getEmbeddedPromptVariant() == EmbeddedPromptVariant.ASK) {
                // Run the necessary delegate callback immediately and
                // schedule the next dialog.
                notifyObservers(ContentSettingValues.BLOCK);
                mDialogDelegate.onDeny();
            } else {
                switch (mDialogDelegate.getEmbeddedPromptVariant()) {
                    case EmbeddedPromptVariant.OS_SYSTEM_SETTINGS -> {
                        notifyObservers(ContentSettingValues.DEFAULT);
                        mDialogDelegate.onAcknowledge();
                    }
                    case EmbeddedPromptVariant.PREVIOUSLY_GRANTED -> {
                        notifyObservers(ContentSettingValues.DEFAULT);
                        mDialogDelegate.onDeny();
                    }
                    case EmbeddedPromptVariant.PREVIOUSLY_DENIED -> {
                        // TODO(crbug.com/388407640): Change it as we might run
                        // into osPrompt variant.
                        notifyObservers(ContentSettingValues.ALLOW);
                        mDialogDelegate.onAcceptThisTime();
                    }
                    default -> {
                        assert false
                                : "Unexpected screen variant in dialog: "
                                        + mDialogDelegate.getEmbeddedPromptVariant();
                    }
                }
            }
            scheduleDisplay();
        } else {
            @DismissalType int type = DismissalType.UNSPECIFIED;
            if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK) {
                type = DismissalType.NAVIGATE_BACK;
            } else if (dismissalCause == DialogDismissalCause.TOUCH_OUTSIDE) {
                type = DismissalType.TOUCH_OUTSIDE;
            }
            notifyObservers(ContentSettingValues.DEFAULT);
            mDialogDelegate.onDismiss(type);
            scheduleDisplay();
        }
    }

    @Override
    public void onClick(PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {
        assert mState == State.PROMPT_OPEN;
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE -> {
                mState = State.PROMPT_POSITIVE_CLICKED;
                handlePositiveButtonClicked(model);
            }
            case ModalDialogProperties.ButtonType.POSITIVE_EPHEMERAL -> {
                mState = State.PROMPT_POSITIVE_EPHEMERAL_CLICKED;
                handlePositiveButtonClicked(model);
            }
            case ModalDialogProperties.ButtonType.NEGATIVE -> {
                mState = State.PROMPT_NEGATIVE_CLICKED;
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
            }
            default -> {
                assert false : "Unexpected button pressed in dialog: " + buttonType;
            }
        }
    }

    public void notifyObservers(@ContentSettingValues int result) {
        if (result != ContentSettingValues.DEFAULT) {
            WindowAndroid currentWindow = mDialogDelegate.getWindow();
            for (Observer obs : mObservers) {
                obs.onDialogResult(
                        currentWindow, mDialogDelegate.getContentSettingsTypes().clone(), result);
            }
        }
    }

    public void acknowledgeDelegate(PropertyModel model) {
        notifyObservers(ContentSettingValues.DEFAULT);
        mDialogDelegate.onAcknowledge();
    }

    private void handlePositiveButtonClicked(PropertyModel model) {
        if (!isEmbeddedPrompt()) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
            return;
        }
        switch (mDialogDelegate.getEmbeddedPromptVariant()) {
            case EmbeddedPromptVariant.ASK -> {
                requestAndroidPermissionsIfNecessary();
            }
            case EmbeddedPromptVariant.ADMINISTRATOR_GRANTED -> {
                acknowledgeDelegate(model);
            }
            case EmbeddedPromptVariant.ADMINISTRATOR_DENIED -> {
                acknowledgeDelegate(model);
            }
            case EmbeddedPromptVariant.OS_SYSTEM_SETTINGS -> {
                // TODO(crbug.com/388407640): ignore missing message and revise the screening logic
                // after accept/deny OS permission
                AndroidPermissionRequester.requestAndroidPermissions(
                        mDialogDelegate.getWindow(),
                        mDialogDelegate.getContentSettingsTypes(),
                        PermissionDialogController.this);
            }
            case EmbeddedPromptVariant.PREVIOUSLY_GRANTED -> {
                acknowledgeDelegate(model);
            }
            case EmbeddedPromptVariant.PREVIOUSLY_DENIED -> {
                acknowledgeDelegate(model);
            }

            default -> {
                assert false
                        : "Unexpected screen variant in dialog: "
                                + mDialogDelegate.getEmbeddedPromptVariant();
            }
        }
    }

    /** Request Android permissions if necessary, after user accepted the dialog */
    private void requestAndroidPermissionsIfNecessary() {
        assert mState == State.PROMPT_POSITIVE_CLICKED
                || mState == State.PROMPT_POSITIVE_EPHEMERAL_CLICKED;
        if (mState == State.PROMPT_POSITIVE_CLICKED) {
            mState = State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT;
        } else {
            mState = State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT;
        }

        // This will call back into either onAndroidPermissionAccepted or
        // onAndroidPermissionCanceled, which will schedule the next permission dialog. If it
        // returns false, no system level permissions need to be requested, so just run the accept
        // callback.
        if (!AndroidPermissionRequester.requestAndroidPermissions(
                mDialogDelegate.getWindow(),
                mDialogDelegate.getContentSettingsTypes(),
                PermissionDialogController.this)) {
            onAndroidPermissionAccepted();
        }
    }

    private boolean isEmbeddedPrompt() {
        return mDialogDelegate != null
                && mDialogDelegate.getEmbeddedPromptVariant()
                        != EmbeddedPromptVariant.UNINITIALIZED;
    }

    public boolean isDialogShownForTest() {
        return mDialogDelegate != null;
    }

    public void clickButtonForTest(@ModalDialogProperties.ButtonType int buttonType) {
        onClick(mDialogModel, buttonType);
    }
}
