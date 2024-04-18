// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;

import org.chromium.base.BuildInfo;
import org.chromium.base.ObserverList;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.LayoutInflaterUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
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
        State.PROMPT_ACCEPTED,
        State.PROMPT_DENIED,
        State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT,
        State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT,
        State.PROMPT_ACCEPTED_THIS_TIME
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        int NOT_SHOWING = 0;
        // We don't show prompts while Chrome Home is showing.
        // int PROMPT_PENDING = 1; // Obsolete.
        int PROMPT_OPEN = 2;
        int PROMPT_ACCEPTED = 3;
        int PROMPT_DENIED = 4;
        int REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT = 5;
        int REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT = 6;
        int PROMPT_ACCEPTED_THIS_TIME = 7;
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

    /** @param observer An observer to be notified of changes. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** @param observer The observer to remove. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Queues a modal permission dialog for display. If there are currently no dialogs on screen, it
     * will be displayed immediately. Otherwise, it will be displayed as soon as the user responds
     * to the current dialog.
     * @param context  The context to use to get the dialog layout.
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
            if (mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT) {
                mDialogDelegate.onAccept();
                destroyDelegate(ContentSettingValues.ALLOW);
            } else {
                // State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT
                mDialogDelegate.onAcceptThisTime();
                destroyDelegate(ContentSettingValues.ALLOW);
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
            // The user accepted the site-level prompt but denied the app-level prompt.
            // No content setting should be set.
            mDialogDelegate.onDismiss(DismissalType.AUTODISMISS_OS_DENIED);
            destroyDelegate(ContentSettingValues.DEFAULT);
        }
        scheduleDisplay();
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
            // TODO(timloh): This probably doesn't work, as this happens synchronously when creating
            // the PermissionPromptAndroid, so the PermissionRequestManager won't be ready yet.
            mDialogDelegate.onDismiss(DismissalType.AUTODISMISS_NO_CONTEXT);
            destroyDelegate(ContentSettingValues.DEFAULT);
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

        // Setting up the MVC model for the dialog's custom view. One time prompts don't share the
        // same layout.
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

        // Setting up the Permission's dialog.
        mDialogModel =
                PermissionDialogModelFactory.getModel(
                        this,
                        mDialogDelegate,
                        customView,
                        () -> showFilteredTouchEventDialog(context));

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
        mState = State.PROMPT_OPEN;
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
                                context.getResources()
                                        .getString(R.string.overlay_detected_dialog_message))
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
        if (mDialogDelegate == delegate) {
            // Some caution is required here to handle cases where the user actions or dismisses
            // the prompt at roughly the same time as native. Due to asynchronicity, this function
            // may be called after onClick and before onDismiss, or before both of those listeners.
            mDialogDelegate = null;
            if (mState == State.PROMPT_OPEN) {
                mModalDialogManager.dismissDialog(
                        mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
            } else {
                assert mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT
                        || mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT
                        || mState == State.PROMPT_DENIED
                        || mState == State.PROMPT_ACCEPTED;
            }
        } else {
            assert mRequestQueue.contains(delegate);
            mRequestQueue.remove(delegate);
        }
        delegate.destroy();
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
        mCustomViewModel = null;
        mCustomViewModelChangeProcessor.destroy();
        if (mDialogDelegate == null) {
            // We get into here if a tab navigates or is closed underneath the
            // prompt.
            mState = State.NOT_SHOWING;
            return;
        }
        if (mState == State.PROMPT_ACCEPTED || mState == State.PROMPT_ACCEPTED_THIS_TIME) {
            if (mState == State.PROMPT_ACCEPTED) {
                mState = State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT;
            } else {
                mState = State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT;
            }

            // Request Android permissions if necessary. This will call back into
            // either onAndroidPermissionAccepted or onAndroidPermissionCanceled,
            // which will schedule the next permission dialog. If it returns false,
            // no system level permissions need to be requested, so just run the
            // accept callback.
            if (!AndroidPermissionRequester.requestAndroidPermissions(
                    mDialogDelegate.getWindow(),
                    mDialogDelegate.getContentSettingsTypes(),
                    PermissionDialogController.this)) {
                onAndroidPermissionAccepted();
            }
        } else {
            // Otherwise, run the necessary delegate callback immediately and
            // schedule the next dialog.
            if (mState == State.PROMPT_DENIED) {
                mDialogDelegate.onCancel();
                destroyDelegate(ContentSettingValues.BLOCK);
            } else {
                assert mState == State.PROMPT_OPEN;

                @DismissalType int type = DismissalType.UNSPECIFIED;
                if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK) {
                    type = DismissalType.NAVIGATE_BACK;
                } else if (dismissalCause == DialogDismissalCause.TOUCH_OUTSIDE) {
                    type = DismissalType.TOUCH_OUTSIDE;
                }
                mDialogDelegate.onDismiss(type);

                destroyDelegate(ContentSettingValues.DEFAULT);
            }
            scheduleDisplay();
        }
    }

    @Override
    public void onClick(PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {
        assert mState == State.PROMPT_OPEN;
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE -> {
                mState = State.PROMPT_ACCEPTED;
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
            }
            case ModalDialogProperties.ButtonType.POSITIVE_EPHEMERAL -> {
                mState = State.PROMPT_ACCEPTED_THIS_TIME;
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
            }
            case ModalDialogProperties.ButtonType.NEGATIVE -> {
                mState = State.PROMPT_DENIED;
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
            }
            default -> {
                assert false : "Unexpected button pressed in dialog: " + buttonType;
            }
        }
    }

    private void destroyDelegate(@ContentSettingValues int result) {
        if (result != ContentSettingValues.DEFAULT) {
            WindowAndroid currentWindow = mDialogDelegate.getWindow();
            for (Observer obs : mObservers) {
                obs.onDialogResult(
                        currentWindow, mDialogDelegate.getContentSettingsTypes().clone(), result);
            }
        }
        mDialogDelegate.destroy();
        mDialogDelegate = null;
        mState = State.NOT_SHOWING;
    }

    public boolean isDialogShownForTest() {
        return mDialogDelegate != null;
    }

    public void clickButtonForTest(@ModalDialogProperties.ButtonType int buttonType) {
        onClick(mDialogModel, buttonType);
    }
}
