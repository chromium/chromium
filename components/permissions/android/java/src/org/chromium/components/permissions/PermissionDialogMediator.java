// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.BuildInfo;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Mediator class Contains the logic for show, dismiss and update a dialog. Controls the dialog
 * state and reacts to UI events.
 */
@NullMarked
public class PermissionDialogMediator
        implements AndroidPermissionRequester.RequestDelegate, ModalDialogProperties.Controller {
    @IntDef({
        State.NOT_SHOWING,
        State.PROMPT_OPEN,
        State.PROMPT_POSITIVE_CLICKED,
        State.PROMPT_NEGATIVE_CLICKED,
        State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT,
        State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT,
        State.PROMPT_POSITIVE_EPHEMERAL_CLICKED,
        State.SHOW_SYSTEM_PROMPT
    })
    @Retention(RetentionPolicy.SOURCE)
    protected @interface State {
        int NOT_SHOWING = 0;
        // We don't show prompts while Chrome Home is showing.
        // int PROMPT_PENDING = 1; // Obsolete.
        int PROMPT_OPEN = 2;
        int PROMPT_POSITIVE_CLICKED = 3;
        int PROMPT_NEGATIVE_CLICKED = 4;
        int REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT = 5;
        int REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT = 6;
        int PROMPT_POSITIVE_EPHEMERAL_CLICKED = 7;
        int SHOW_SYSTEM_PROMPT = 8;
    }

    protected @Nullable PropertyModel mDialogModel;
    private @Nullable PropertyModel mOverlayDetectedDialogModel;
    protected @Nullable PermissionDialogDelegate mDialogDelegate;
    protected @Nullable ModalDialogManager mModalDialogManager;
    protected PermissionDialogCoordinator.@Nullable Delegate mCoordinatorDelegate;

    /** The current state, whether we have a prompt showing and so on. */
    protected @State int mState;

    public PermissionDialogMediator(PermissionDialogCoordinator.Delegate delegate) {
        mCoordinatorDelegate = delegate;
        mState = State.NOT_SHOWING;
    }

    /**
     * Shows the dialog asking the user for a web API permission.
     *
     * @param delegate The wrapper for the native-side permission delegate.
     * @param manager The {@link ModalDialogManager} using to show permission dialog.
     * @param view The {@link View} custom view going to be shown on permission dialog.
     */
    public void showDialog(
            PermissionDialogDelegate delegate, ModalDialogManager manager, View view) {
        assert mState == State.NOT_SHOWING;
        mDialogDelegate = delegate;
        mModalDialogManager = manager;
        mDialogModel = createModalDialogModel(view);
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
        mState = State.PROMPT_OPEN;
    }

    /** Update the current displaying dialog. */
    public void updateDialog(View customView) {
        assert false; // NOTREACHED
    }

    /** Dismiss the current dialog, called from native. */
    public void dismissFromNative() {
        // Some caution is required here to handle cases where the user actions or dismisses
        // the prompt at roughly the same time as native. Due to asynchronicity, this
        // function may be called after onClick and before onDismiss, or before both of
        // those listeners.
        if (mState == State.PROMPT_OPEN) {
            assumeNonNull(mModalDialogManager)
                    .dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
        } else {
            assert mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT
                    || mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT
                    || mState == State.PROMPT_NEGATIVE_CLICKED
                    || mState == State.PROMPT_POSITIVE_CLICKED;
            onPermissionDialogEnded();
        }
    }

    /**
     * Setting up the modal dialog model for the permission's dialog.
     *
     * @param view The {@link View} custom view going to be shown on permission dialog.
     */
    protected PropertyModel createModalDialogModel(View view) {
        return PermissionDialogModelFactory.getModel(
                this,
                assumeNonNull(mDialogDelegate),
                view,
                () -> showFilteredTouchEventDialog(getContext()));
    }

    /**
     * Displays the dialog explaining that Chrome has detected an overlay. Offers the user to close
     * the overlay window and try again.
     */
    private void showFilteredTouchEventDialog(Context context) {
        // Don't show another dialog if one is already displayed.
        if (mOverlayDetectedDialogModel != null) return;

        assumeNonNull(mModalDialogManager);
        ModalDialogProperties.Controller overlayDetectedDialogController =
                new SimpleModalDialogController(
                        mModalDialogManager,
                        (Integer dismissalCause) -> {
                            if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                    && mDialogModel != null) {
                                assumeNonNull(mModalDialogManager)
                                        .dismissDialog(
                                                mDialogModel,
                                                DialogDismissalCause
                                                        .NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
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

    @Override
    public void onAndroidPermissionAccepted() {
        // The tab may have navigated or been closed behind the Android permission prompt.
        if (mDialogDelegate == null) {
            mState = State.NOT_SHOWING;
        } else {
            assert mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT
                    || mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT;

            onPermissionDialogResult(ContentSettingValues.ALLOW);
            if (mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT) {
                mDialogDelegate.onAccept();
            } else {
                // State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT
                mDialogDelegate.onAcceptThisTime();
            }
        }
        onPermissionDialogEnded();
    }

    @Override
    public void onAndroidPermissionCanceled() {
        assert mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT
                || mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT;

        // The tab may have navigated or been closed behind the Android permission prompt.
        if (mDialogDelegate == null) {
            mState = State.NOT_SHOWING;
        } else {
            onPermissionDialogResult(ContentSettingValues.DEFAULT);
            // The user accepted the site-level prompt but denied the app-level prompt.
            // No content setting should be set.
            mDialogDelegate.onDismiss(DismissalType.AUTODISMISS_OS_DENIED);
        }
        onPermissionDialogEnded();
    }

    @Override
    public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
        // Called when the dialog is dismissed. Interacting with any button in the dialog will
        // call this handler after its own handler.
        // When the dialog is dismissed, the delegate's native pointers are
        // freed, and the next queued dialog (if any) is displayed.
        if (mDialogDelegate == null || dismissalCause == DialogDismissalCause.DISMISSED_BY_NATIVE) {
            // We get into here if a tab navigates or is closed underneath the
            // prompt.
            onPermissionDialogEnded();
            return;
        }

        // It's possible to dismiss the dialog and show OS prompt. In this case, do nothing
        if (mState == State.SHOW_SYSTEM_PROMPT) {
            return;
        }

        if (mState == State.PROMPT_POSITIVE_EPHEMERAL_CLICKED) {
            handleDismissPositiveButtonClickedState();
        } else if (mState == State.PROMPT_POSITIVE_CLICKED) {
            handleDismissPositiveButtonClickedState();
        } else if (mState == State.PROMPT_NEGATIVE_CLICKED) {
            handleDismissNegativeButtonClickedState();
        } else {
            @DismissalType int type = DismissalType.UNSPECIFIED;
            if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK) {
                type = DismissalType.NAVIGATE_BACK;
            } else if (dismissalCause == DialogDismissalCause.TOUCH_OUTSIDE) {
                type = DismissalType.TOUCH_OUTSIDE;
            }
            onPermissionDialogResult(ContentSettingValues.DEFAULT);
            mDialogDelegate.onDismiss(type);
            onPermissionDialogEnded();
        }
    }

    @Override
    public final void onClick(
            PropertyModel model, @ModalDialogProperties.ButtonType int buttonType) {
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
                handleNegativeButtonClicked(model);
            }
            default -> {
                assert false : "Unexpected button pressed in dialog: " + buttonType;
            }
        }
    }

    /** Handle positive button clicked, right after user click on that button */
    protected void handlePositiveButtonClicked(PropertyModel model) {
        if (mModalDialogManager != null) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        }
    }

    /** Handle negative button clicked, right after user click on that button */
    protected void handleNegativeButtonClicked(PropertyModel model) {
        if (mModalDialogManager != null) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    /** Handle negative button clicked state, after dialog is dismissed */
    protected void handleDismissNegativeButtonClickedState() {
        // Run the necessary delegate callback immediately and will schedule the next dialog.
        onPermissionDialogResult(ContentSettingValues.BLOCK);
        assumeNonNull(mDialogDelegate).onDeny();
        onPermissionDialogEnded();
    }

    /** Handle positive button clicked state, after dialog is dismissed */
    protected void handleDismissPositiveButtonClickedState() {
        requestAndroidPermissionsIfNecessary();
    }

    /** Request Android permissions if necessary, after user accepted the dialog */
    protected void requestAndroidPermissionsIfNecessary() {
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
        assumeNonNull(mDialogDelegate);
        if (!AndroidPermissionRequester.requestAndroidPermissions(
                mDialogDelegate.getWindow(),
                mDialogDelegate.getContentSettingsTypes(),
                PermissionDialogMediator.this)) {
            onAndroidPermissionAccepted();
        }
    }

    /** Notify that the permissions prompting flow is already ended */
    protected void onPermissionDialogEnded() {
        if (mCoordinatorDelegate != null) {
            mCoordinatorDelegate.onPermissionDialogEnded();
        }
    }

    /** Notify that user has just completed a permissions prompt flow with a result */
    protected void onPermissionDialogResult(@ContentSettingValues int result) {
        if (mCoordinatorDelegate != null) {
            mCoordinatorDelegate.onPermissionDialogResult(result);
        }
    }

    protected Context getContext() {
        assert mDialogDelegate != null;
        Context context = mDialogDelegate.getWindow().getContext().get();
        assert context != null;
        return context;
    }

    public void destroy() {
        if (mModalDialogManager != null) {
            mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.UNKNOWN);
        }

        mDialogModel = null;
        mDialogDelegate = null;
        mModalDialogManager = null;
        mCoordinatorDelegate = null;
        mState = State.NOT_SHOWING;
    }

    public void clickButtonForTest(@ModalDialogProperties.ButtonType int buttonType) {
        onClick(assumeNonNull(mDialogModel), buttonType);
    }
}
