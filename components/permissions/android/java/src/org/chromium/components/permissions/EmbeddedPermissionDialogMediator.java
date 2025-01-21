// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class to handle logic of embedded permission dialog */
public class EmbeddedPermissionDialogMediator extends PermissionDialogMediator {

    public EmbeddedPermissionDialogMediator(PermissionDialogCoordinator.Delegate delegate) {
        super(delegate);
    }

    @Override
    public void dismissFromNative() {
        mDialogDelegate = null;
        mModalDialogManager.dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
        mCoordinatorDelegate.onPermissionDialogEnded();
    }

    @Override
    public void onAndroidPermissionAccepted() {
        // The tab may have navigated or been closed behind the Android permission prompt.
        if (mDialogDelegate == null) {
            mState = State.NOT_SHOWING;
        } else {
            assert mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT
                    || mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT;
            // We will not notify `onPermissionDialogResult` here. If this dialog comes from a
            // embedded permission request, it might have multiple of
            // different screens showing up. It makes more sense to notify the observers (in this
            // case to change the icon) after all the screens have appeared.
            if (mState == State.REQUEST_ANDROID_PERMISSIONS_FOR_PERSISTENT_GRANT) {
                mDialogDelegate.onAccept();
            } else {
                // State.REQUEST_ANDROID_PERMISSIONS_FOR_EPHEMERAL_GRANT
                mDialogDelegate.onAcceptThisTime();
            }
        }
        // TODO(crbug.com/388407640): We might need to update screen, not end here.
        mCoordinatorDelegate.onPermissionDialogEnded();
    }

    private void acknowledgeDelegate() {
        mCoordinatorDelegate.onPermissionDialogResult(ContentSettingValues.DEFAULT);
        mDialogDelegate.onAcknowledge();
    }

    @Override
    protected final void handlePositiveButtonClicked(PropertyModel model) {
        switch (mDialogDelegate.getEmbeddedPromptVariant()) {
            case EmbeddedPromptVariant.ASK -> {
                requestAndroidPermissionsIfNecessary();
            }
            case EmbeddedPromptVariant.ADMINISTRATOR_GRANTED -> {
                acknowledgeDelegate();
            }
            case EmbeddedPromptVariant.ADMINISTRATOR_DENIED -> {
                acknowledgeDelegate();
            }
            case EmbeddedPromptVariant.OS_SYSTEM_SETTINGS -> {
                // TODO(crbug.com/388407640): ignore missing message and revise the screening logic
                // after accept/deny OS permission
                AndroidPermissionRequester.requestAndroidPermissions(
                        mDialogDelegate.getWindow(),
                        mDialogDelegate.getContentSettingsTypes(),
                        EmbeddedPermissionDialogMediator.this);
            }
            case EmbeddedPromptVariant.PREVIOUSLY_GRANTED -> {
                acknowledgeDelegate();
            }
            case EmbeddedPromptVariant.PREVIOUSLY_DENIED -> {
                acknowledgeDelegate();
            }

            default -> {
                assert false
                        : "Unexpected screen variant in dialog: "
                                + mDialogDelegate.getEmbeddedPromptVariant();
            }
        }
    }

    /** Handle negative button clicked state, after dialog is dismissed */
    @Override
    protected void handleDismissNegativeButtonClickedState() {
        switch (mDialogDelegate.getEmbeddedPromptVariant()) {
            case EmbeddedPromptVariant.ASK -> {
                mCoordinatorDelegate.onPermissionDialogResult(ContentSettingValues.BLOCK);
                mDialogDelegate.onDeny();
            }
            case EmbeddedPromptVariant.OS_SYSTEM_SETTINGS -> {
                mCoordinatorDelegate.onPermissionDialogResult(ContentSettingValues.DEFAULT);
                mDialogDelegate.onAcknowledge();
            }
            case EmbeddedPromptVariant.PREVIOUSLY_GRANTED -> {
                mCoordinatorDelegate.onPermissionDialogResult(ContentSettingValues.DEFAULT);
                mDialogDelegate.onDeny();
            }
            case EmbeddedPromptVariant.PREVIOUSLY_DENIED -> {
                // TODO(crbug.com/388407640): Change it as we might run
                // into osPrompt variant.
                mCoordinatorDelegate.onPermissionDialogResult(ContentSettingValues.ALLOW);
                mDialogDelegate.onAcceptThisTime();
            }
            default -> {
                assert false
                        : "Unexpected screen variant in dialog: "
                                + mDialogDelegate.getEmbeddedPromptVariant();
            }
        }
        mCoordinatorDelegate.onPermissionDialogEnded();
    }

    /** Handle positive button clicked state, after dialog is dismissed */
    @Override
    protected void handleDismissPositiveButtonClickedState() {
        // Request android permission in embedded prompt in `PROMPT_POSITIVE_CLICKED` state will
        // be handled in onClick.
        assert false; // NOTREACHED
    }
}
