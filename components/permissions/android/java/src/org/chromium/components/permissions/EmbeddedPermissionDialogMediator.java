// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.permissions.PermissionUtil.getGeolocationType;

import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;
import android.view.View;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.location.LocationUtils;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class to handle logic of embedded permission dialog */
@NullMarked
public class EmbeddedPermissionDialogMediator extends PermissionDialogMediator
        implements ActivityStateObserver {
    public EmbeddedPermissionDialogMediator(PermissionDialogCoordinator.Delegate delegate) {
        super(delegate);
    }

    @Override
    public void showDialog(
            PermissionDialogDelegate delegate, ModalDialogManager manager, View view) {
        mDialogDelegate = delegate;
        mModalDialogManager = manager;
        mDialogDelegate.getWindow().addActivityStateObserver(this);
        // We will show the OS prompt setting directly, skip calling
        // `mModalDialogManager.showDialog` and `PROMPT_OPEN` state.
        if (mDialogDelegate.getEmbeddedPromptVariant() == EmbeddedPromptVariant.OS_PROMPT) {
            mState = State.SHOW_SYSTEM_PROMPT;
            requestAndroidPermissionsIfNecessary();
            return;
        }
        showDialogInternal(view);
    }

    public void showDialogInternal(View view) {
        assert mState == State.NOT_SHOWING || mState == State.SHOW_SYSTEM_PROMPT;
        mDialogModel = createModalDialogModel(view);
        assumeNonNull(mModalDialogManager)
                .showDialog(mDialogModel, ModalDialogManager.ModalDialogType.TAB);
        mState = State.PROMPT_OPEN;
    }

    @Override
    public void updateDialog(View customView) {
        if (mState == State.NOT_SHOWING || mState == State.SHOW_SYSTEM_PROMPT) {
            showDialogInternal(customView);
        } else if (assumeNonNull(mDialogDelegate).getEmbeddedPromptVariant()
                == EmbeddedPromptVariant.OS_PROMPT) {
            mState = State.SHOW_SYSTEM_PROMPT;
            assumeNonNull(mModalDialogManager)
                    .dismissDialog(mDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
            requestAndroidPermissionsIfNecessary();
        } else {
            mState = State.PROMPT_OPEN;
            assumeNonNull(mDialogModel);
            if (PermissionDialogModelFactory.shouldUseVerticalButtons(mDialogDelegate)) {
                mDialogModel.set(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, true);
                mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, new String());
                mDialogModel.set(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, new String());
                mDialogModel.set(
                        ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST,
                        PermissionDialogModelFactory.getButtonSpecs(mDialogDelegate));
            } else {
                mDialogModel.set(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, false);
                // @NullUnmarked because PropertyModel values are marked as non-null.
                mDialogModel.set(ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST, null);
                mDialogModel.set(
                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        mDialogDelegate.getPositiveButtonText());
                mDialogModel.set(
                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        mDialogDelegate.getNegativeButtonText());
            }
            mDialogModel.set(ModalDialogProperties.CUSTOM_VIEW, customView);
        }
    }

    @Override
    public void dismissFromNative() {
        assumeNonNull(mModalDialogManager)
                .dismissDialog(mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
        onPermissionDialogEnded();
    }

    @Override
    public void onAndroidPermissionAccepted() {
        handleSystemPermission(/*accepted*/ true);
    }

    @Override
    public void onAndroidPermissionCanceled() {
        handleSystemPermission(/*accepted*/ false);
    }

    private void acknowledgeDelegate() {
        onPermissionDialogResult(ContentSetting.DEFAULT);
        assumeNonNull(mDialogDelegate).onAcknowledge();
    }

    private void denyDelegate() {
        onPermissionDialogResult(ContentSetting.BLOCK);
        assumeNonNull(mDialogDelegate).onDeny();
    }

    private void resumeDelegate() {
        assumeNonNull(mDialogDelegate).onResume();
    }

    private void onSystemSettingsShownDelegate() {
        assumeNonNull(mDialogDelegate).onSystemSettingsShown();
    }

    // We will not notify `onPermissionDialogResult` in `accept.*Delegate`. If this dialog comes
    // from an embedded permission request, it might have multiple of different screens showing up.
    // It makes more sense to notify the observers (in this case to change the icon) after all the
    // screens have appeared.
    private void acceptDelegate() {
        assumeNonNull(mDialogDelegate).onAccept();
    }

    private void acceptThisTimeDelegate() {
        assumeNonNull(mDialogDelegate).onAcceptThisTime();
    }

    private void handleSystemPermission(boolean accepted) {
        // The tab may have navigated or been closed behind the Android permission prompt.
        if (mDialogDelegate == null) {
            onPermissionDialogEnded();
            return;
        }

        mDialogDelegate.onSystemPermissionResolved(accepted);
    }

    @Override
    protected final void handlePositiveButtonClicked(PropertyModel model) {
        if (mDialogDelegate == null) {
            return;
        }

        switch (mDialogDelegate.getEmbeddedPromptVariant()) {
            case EmbeddedPromptVariant.ASK -> {
                if (mState == State.PROMPT_POSITIVE_CLICKED) {
                    acceptDelegate();
                } else {
                    acceptThisTimeDelegate();
                }
            }
            case EmbeddedPromptVariant.OS_SYSTEM_SETTINGS -> {
                var type = mDialogDelegate.getContentSettingsTypes()[0];
                Intent intent =
                        (type == getGeolocationType())
                                ? getLocationSettingsIntent()
                                : getAppInfoSettingsIntent();
                if (!mDialogDelegate.getWindow().canResolveActivity(intent)) {
                    intent = getGlobalSettingsIntent();
                }
                onSystemSettingsShownDelegate();
                getContext().startActivity(intent);
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

    @Override
    protected void handleNegativeButtonClicked(PropertyModel model) {
        if (mDialogDelegate == null) {
            return;
        }
        switch (mDialogDelegate.getEmbeddedPromptVariant()) {
            case EmbeddedPromptVariant.ASK -> {
                // There is no X button on Android. The negative button here is an extra option
                // providing to users so they can temporarily block the permission (identical to
                // dismiss).
                acknowledgeDelegate();
            }
            case EmbeddedPromptVariant.OS_SYSTEM_SETTINGS -> {
                acknowledgeDelegate();
            }
            case EmbeddedPromptVariant.PREVIOUSLY_GRANTED -> {
                denyDelegate();
            }
            case EmbeddedPromptVariant.PREVIOUSLY_DENIED -> {
                acceptThisTimeDelegate();
            }
            case EmbeddedPromptVariant.ADMINISTRATOR_GRANTED -> {
                acknowledgeDelegate();
            }
            case EmbeddedPromptVariant.ADMINISTRATOR_DENIED -> {
                acknowledgeDelegate();
            }

            default -> {
                assert false
                        : "Unexpected screen variant in dialog: "
                                + mDialogDelegate.getEmbeddedPromptVariant();
            }
        }
    }

    @Override
    protected void handleDismissNegativeButtonClickedState() {}

    @Override
    protected void handleDismissPositiveButtonClickedState() {}

    @Override
    protected void requestAndroidPermissionsIfNecessary() {
        // This will call back into either:
        // - onAndroidPermissionAccepted or onAndroidPermissionCanceled, which will schedule the
        // next permission dialog:
        // - Missing permission runnalble, in this case is to acknowledge the dialog.
        // If it returns false, no system level permissions need to be requested, so just run the
        // accept callback.
        assumeNonNull(mDialogDelegate);
        if (!AndroidPermissionRequester.requestAndroidPermissions(
                mDialogDelegate.getWindow(),
                mDialogDelegate.getContentSettingsTypes(),
                EmbeddedPermissionDialogMediator.this,
                () -> {
                    onAndroidPermissionCanceled();
                })) {
            onAndroidPermissionAccepted();
        }
    }

    @Override
    public void onActivityResumed() {
        if (mDialogDelegate != null) {
            resumeDelegate();
        }
    }

    @Override
    public final void destroy() {
        if (mDialogDelegate != null) {
            mDialogDelegate.getWindow().removeActivityStateObserver(this);
        }
        super.destroy();
    }

    /**
     * This returns an intent to show the Android Location Settings. If the location service is off
     * for the whole device, it'll show the location source settings; otherwise, it shows the App
     * details Settings.
     */
    private Intent getLocationSettingsIntent() {
        if (LocationUtils.getInstance().isSystemLocationSettingEnabled()) {
            return getAppInfoSettingsIntent();
        }
        Intent intent = new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    /** Returns an intent to show System Settings. */
    private Intent getGlobalSettingsIntent() {
        Intent intent = new Intent(Settings.ACTION_SETTINGS);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    /** Returns an intent to show the Application Details Settings. */
    private Intent getAppInfoSettingsIntent() {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.setData(
                Uri.parse("package:" + ContextUtils.getApplicationContext().getPackageName()));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    public @Nullable PermissionDialogDelegate getDelegateForTest() {
        return mDialogDelegate;
    }
}
