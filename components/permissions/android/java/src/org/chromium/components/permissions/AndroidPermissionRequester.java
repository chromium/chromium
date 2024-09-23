// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.content.Context;
import android.content.pm.PackageManager;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.jni_zero.CalledByNative;

import org.chromium.base.BuildInfo;
import org.chromium.base.CollectionUtil;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;

import java.util.HashSet;
import java.util.Set;
import java.util.function.Consumer;

/**
 * Methods to handle requesting native permissions from Android when the user grants a website a
 * permission.
 */
public class AndroidPermissionRequester {
    /**
     * An interface for classes which need to be informed of the outcome of asking a user to grant
     * an Android permission.
     */
    public interface RequestDelegate {
        void onAndroidPermissionAccepted();

        void onAndroidPermissionCanceled();
    }

    private static Set<String> filterPermissionsKeepMissing(
            AndroidPermissionDelegate permissionDelegate, String[] androidPermissions) {
        Set<String> missingAndroidPermissions = new HashSet<String>();
        for (String permission : androidPermissions) {
            if (!permissionDelegate.hasPermission(permission)) {
                missingAndroidPermissions.add(permission);
            }
        }
        return missingAndroidPermissions;
    }

    private static int getContentSettingType(
            SparseArray<Set<String>> contentSettingsTypesToPermissionsMap, String permission) {
        // SparseArray#indexOfValue uses == instead of .equals, so we need to manually iterate
        // over the list.
        for (int i = 0; i < contentSettingsTypesToPermissionsMap.size(); i++) {
            final Set<String> contentSettingPermissions =
                    contentSettingsTypesToPermissionsMap.valueAt(i);
            if (contentSettingPermissions.contains(permission)) {
                return contentSettingsTypesToPermissionsMap.keyAt(i);
            }
        }

        return -1;
    }

    /**
     * Determines whether the minimum required Android permissions are granted for the specified
     * content setting.
     *
     * @param permissionDelegate The AndroidPermissionDelegate used to determine permission status.
     * @param contentSettingsType The content setting whose permissions are being checked.
     * @return Whether the necessary permissions are granted for the given content setting.
     */
    @CalledByNative
    public static boolean hasRequiredAndroidPermissionsForContentSetting(
            AndroidPermissionDelegate permissionDelegate,
            @ContentSettingsType.EnumType int contentSettingsType) {
        Set<String> missingPermissions =
                filterPermissionsKeepMissing(
                        permissionDelegate,
                        PermissionUtil.getRequiredAndroidPermissionsForContentSetting(
                                contentSettingsType));

        // TODO(crbug.com/40765216): AndroidPermissionDelegate.hasPermission has side effects that
        // allows users to recover from states where they had previously denied the permission, by
        // virtue of clearing a Chrome-side shared preference instructing Chrome not to prompt again
        // again. Ensure here that these prefs get cleared for optional permissions as well.
        String[] optionalPermissions =
                PermissionUtil.getOptionalAndroidPermissionsForContentSetting(contentSettingsType);
        for (String permission : optionalPermissions) {
            boolean unused_result = permissionDelegate.hasPermission(permission);
        }

        return missingPermissions.isEmpty();
    }

    /**
     * Returns true if any of the permissions in contentSettingsTypes must be requested from the
     * system. Otherwise returns false.
     *
     * If true is returned, this method will asynchronously request the necessary permissions using
     * a dialog, running methods on the RequestDelegate when the user has made a decision.
     */
    public static boolean requestAndroidPermissions(
            final WindowAndroid windowAndroid,
            final int[] contentSettingsTypes,
            final RequestDelegate delegate) {
        if (windowAndroid == null) return false;

        SparseArray<Set<String>> contentSettingsTypesToRequiredPermissionsMap = new SparseArray<>();
        Set<String> allPermissionsToRequest = new HashSet<>();
        for (int contentSettingType : contentSettingsTypes) {
            if (hasRequiredAndroidPermissionsForContentSetting(windowAndroid, contentSettingType)) {
                continue;
            }

            final Set<String> requiredPermissions =
                    CollectionUtil.newHashSet(
                            PermissionUtil.getRequiredAndroidPermissionsForContentSetting(
                                    contentSettingType));
            final Set<String> optionalPermissions =
                    CollectionUtil.newHashSet(
                            PermissionUtil.getOptionalAndroidPermissionsForContentSetting(
                                    contentSettingType));

            contentSettingsTypesToRequiredPermissionsMap.append(
                    contentSettingType, requiredPermissions);
            allPermissionsToRequest.addAll(requiredPermissions);
            allPermissionsToRequest.addAll(optionalPermissions);
        }

        if (allPermissionsToRequest.isEmpty()) {
            return false;
        }

        PermissionCallback callback =
                new PermissionCallback() {
                    @Override
                    public void onRequestPermissionsResult(
                            String[] permissions, int[] grantResults) {
                        boolean allRequestable = true;
                        Set<Integer> deniedContentSettings = new HashSet<Integer>();

                        for (int i = 0; i < grantResults.length; i++) {
                            if (grantResults[i] == PackageManager.PERMISSION_DENIED) {
                                final int deniedContentSetting =
                                        getContentSettingType(
                                                contentSettingsTypesToRequiredPermissionsMap,
                                                permissions[i]);
                                // Never mind if an optional Android permission was denied.
                                if (deniedContentSetting == -1) {
                                    continue;
                                }
                                deniedContentSettings.add(deniedContentSetting);
                                if (!windowAndroid.canRequestPermission(permissions[i])) {
                                    allRequestable = false;
                                }
                            }
                        }

                        Context context = windowAndroid.getContext().get();

                        if (allRequestable && !deniedContentSettings.isEmpty() && context != null) {
                            int deniedStringId = -1;
                            if (deniedContentSettings.size() == 2
                                    && deniedContentSettings.contains(
                                            ContentSettingsType.MEDIASTREAM_MIC)
                                    && deniedContentSettings.contains(
                                            ContentSettingsType.MEDIASTREAM_CAMERA)) {
                                deniedStringId =
                                        R.string.infobar_missing_microphone_camera_permissions_text;
                            } else if (deniedContentSettings.size() == 1) {
                                if (deniedContentSettings.contains(
                                        ContentSettingsType.GEOLOCATION)) {
                                    deniedStringId =
                                            R.string.infobar_missing_location_permission_text;
                                } else if (deniedContentSettings.contains(
                                        ContentSettingsType.MEDIASTREAM_MIC)) {
                                    deniedStringId =
                                            R.string.infobar_missing_microphone_permission_text;
                                } else if (deniedContentSettings.contains(
                                        ContentSettingsType.MEDIASTREAM_CAMERA)) {
                                    deniedStringId =
                                            R.string.infobar_missing_camera_permission_text;
                                } else if (deniedContentSettings.contains(
                                        ContentSettingsType.HAND_TRACKING)) {
                                    deniedStringId =
                                            R.string.infobar_missing_hand_tracking_permission_text;
                                } else if (deniedContentSettings.contains(ContentSettingsType.AR)) {
                                    deniedStringId =
                                            R.string.infobar_missing_ar_camera_permission_text;
                                } else if (deniedContentSettings.contains(
                                        ContentSettingsType.NOTIFICATIONS)) {
                                    // We don't want to request the notification prompt again, since
                                    // user declined it already.
                                    delegate.onAndroidPermissionCanceled();
                                    return;
                                }
                            }

                            assert deniedStringId != -1
                                    : "Invalid combination of missing content settings: "
                                            + deniedContentSettings;

                            String appName = BuildInfo.getInstance().hostPackageLabel;
                            showMissingPermissionDialog(
                                    windowAndroid,
                                    context.getString(deniedStringId, appName),
                                    (model) -> {
                                        final ModalDialogManager modalDialogManager =
                                                windowAndroid.getModalDialogManager();
                                        modalDialogManager.dismissDialog(
                                                model,
                                                DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                                        requestAndroidPermissions(
                                                windowAndroid, contentSettingsTypes, delegate);
                                    },
                                    delegate::onAndroidPermissionCanceled);
                        } else if (deniedContentSettings.isEmpty()) {
                            delegate.onAndroidPermissionAccepted();
                        } else {
                            delegate.onAndroidPermissionCanceled();
                        }
                    }
                };

        windowAndroid.requestPermissions(
                allPermissionsToRequest.toArray(new String[allPermissionsToRequest.size()]),
                callback);
        return true;
    }

    /**
     * Shows a dialog that informs the user about a missing Android permission. Note that
     * the dialog is not dismissed when the positive button is clicked, rather it will be
     * dismissed after the Android permissions dialog is dismissed.
     * @param windowAndroid Current WindowAndroid.
     * @param messageId The message that is shown on the dialog.
     * @param onPositiveButtonClicked Consumer that is executed on positive button click.
     *         It takes a PropertyModel.
     * @param onCancelled Runnable that is executed on cancellation.
     */
    public static void showMissingPermissionDialog(
            WindowAndroid windowAndroid,
            String message,
            Consumer<PropertyModel> onPositiveButtonClicked,
            Runnable onCancelled) {
        final ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
        assert modalDialogManager != null : "ModalDialogManager is null";

        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            onPositiveButtonClicked.accept(model);
                        }
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                            onCancelled.run();
                        }
                    }
                };
        Context context = windowAndroid.getContext().get();
        View view = LayoutInflater.from(context).inflate(R.layout.update_permissions_dialog, null);
        TextView dialogText = view.findViewById(R.id.text);
        dialogText.setText(message);
        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CUSTOM_VIEW, view)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                context.getString(R.string.infobar_update_permissions_button_text))
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .build();
        modalDialogManager.showDialog(dialogModel, ModalDialogManager.ModalDialogType.APP);
    }
}
