// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.Manifest;
import android.os.Build;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.webxr.WebXrAndroidFeatureMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.vr.XrFeatureStatus;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.ContextualNotificationPermissionRequester;
import org.chromium.ui.permissions.PermissionCallback;

import java.util.Arrays;

/** A utility class for permissions. */
@NullMarked
public class PermissionUtil {
    /**
     * TODO(https://crbug.com/331574787): Replace with official strings. At which time, any
     * additional checks being done to guard this with the immersive feature can likely also be
     * removed.
     */
    public static final String ANDROID_PERMISSION_SCENE_UNDERSTANDING_FINE =
            "android.permission.SCENE_UNDERSTANDING_FINE";

    public static final String ANDROID_PERMISSION_HAND_TRACKING =
            "android.permission.HAND_TRACKING";

    /** The permissions associated with requesting location pre-Android S. */
    private static final String[] LOCATION_PERMISSIONS_PRE_S = {
        android.Manifest.permission.ACCESS_FINE_LOCATION,
        android.Manifest.permission.ACCESS_COARSE_LOCATION
    };

    /** The required Android permissions associated with requesting location post-Android S. */
    private static final String[] LOCATION_REQUIRED_PERMISSIONS_POST_S = {
        android.Manifest.permission.ACCESS_COARSE_LOCATION
    };

    /** The optional Android permissions associated with requesting location post-Android S. */
    private static final String[] LOCATION_OPTIONAL_PERMISSIONS_POST_S = {
        android.Manifest.permission.ACCESS_FINE_LOCATION
    };

    /** The android permissions associated with requesting access to the camera. */
    private static final String[] CAMERA_PERMISSIONS = {android.Manifest.permission.CAMERA};

    /** The android permissions associated with requesting access to the microphone. */
    private static final String[] MICROPHONE_PERMISSIONS = {
        android.Manifest.permission.RECORD_AUDIO
    };

    /** The required android permissions associated with posting notifications post-Android T. */
    private static final String[] NOTIFICATION_PERMISSIONS_POST_T = {
        android.Manifest.permission.POST_NOTIFICATIONS
    };

    private static final String[] OPENXR_PERMISSIONS = {
        ANDROID_PERMISSION_SCENE_UNDERSTANDING_FINE
    };

    private static final String[] HAND_TRACKING_PERMISSIONS = {ANDROID_PERMISSION_HAND_TRACKING};

    /** Signifies there are no permissions associated. */
    private static final String[] EMPTY_PERMISSIONS = {};

    private PermissionUtil() {}

    /** Whether precise/approximate location support is enabled. */
    private static boolean isApproximateLocationSupportEnabled() {
        // Even for apps targeting SDK version 30-, the user can downgrade location precision
        // in app settings if the device is running Android S. In addition, for apps targeting SDK
        // version 31+, users will be able to choose the precision in the permission dialog for apps
        // targeting SDK version 31. Therefore enable support based on the current device's
        // software's SDK version as opposed to Chrome's targetSdkVersion. See:
        // https://developer.android.com/about/versions/12/approximate-location
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S;
    }

    private static boolean openXrNeedsAdditionalPermissions() {
        // OpenXR only requires additional permissions after Android 14.
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE
                && XrFeatureStatus.isXrDevice()
                && WebXrAndroidFeatureMap.isOpenXrEnabled();
    }

    public static boolean handTrackingNeedsAdditionalPermissions() {
        return XrFeatureStatus.isXrDevice() && WebXrAndroidFeatureMap.isHandTrackingEnabled();
    }

    /**
     * Returns required Android permission strings for a given {@link ContentSettingsType}. If there
     * is no permissions associated with the content setting, then an empty array is returned.
     *
     * @param contentSettingType The content setting to get the Android permissions for.
     * @return The required Android permissions for the given content setting. Permission sets
     *     returned for different content setting types are disjunct.
     */
    @CalledByNative
    public static String[] getRequiredAndroidPermissionsForContentSetting(int contentSettingType) {
        switch (contentSettingType) {
            case ContentSettingsType.GEOLOCATION, ContentSettingsType.GEOLOCATION_WITH_OPTIONS:
                if (isApproximateLocationSupportEnabled()) {
                    return Arrays.copyOf(
                            LOCATION_REQUIRED_PERMISSIONS_POST_S,
                            LOCATION_REQUIRED_PERMISSIONS_POST_S.length);
                }
                return Arrays.copyOf(LOCATION_PERMISSIONS_PRE_S, LOCATION_PERMISSIONS_PRE_S.length);
            case ContentSettingsType.MEDIASTREAM_MIC:
                return Arrays.copyOf(MICROPHONE_PERMISSIONS, MICROPHONE_PERMISSIONS.length);
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                return Arrays.copyOf(CAMERA_PERMISSIONS, CAMERA_PERMISSIONS.length);
            case ContentSettingsType.AR:
                if (openXrNeedsAdditionalPermissions()) {
                    return Arrays.copyOf(OPENXR_PERMISSIONS, OPENXR_PERMISSIONS.length);
                }
                return Arrays.copyOf(CAMERA_PERMISSIONS, CAMERA_PERMISSIONS.length);
            case ContentSettingsType.VR:
                if (openXrNeedsAdditionalPermissions()) {
                    return Arrays.copyOf(OPENXR_PERMISSIONS, OPENXR_PERMISSIONS.length);
                }
                return EMPTY_PERMISSIONS;
            case ContentSettingsType.HAND_TRACKING:
                if (handTrackingNeedsAdditionalPermissions()) {
                    return Arrays.copyOf(
                            HAND_TRACKING_PERMISSIONS, HAND_TRACKING_PERMISSIONS.length);
                }
                return EMPTY_PERMISSIONS;
            case ContentSettingsType.NOTIFICATIONS:
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    return Arrays.copyOf(
                            NOTIFICATION_PERMISSIONS_POST_T,
                            NOTIFICATION_PERMISSIONS_POST_T.length);
                }
                return EMPTY_PERMISSIONS;
            default:
                return EMPTY_PERMISSIONS;
        }
    }

    /**
     * Returns optional Android permission strings for a given {@link ContentSettingsType}. If there
     * is no permissions associated with the content setting, or all of them are required, then an
     * empty array is returned.
     *
     * @param contentSettingType The content setting to get the Android permissions for.
     * @return The optional Android permissions for the given content setting. Permission sets
     *     returned for different content setting types are disjunct.
     */
    @CalledByNative
    public static String[] getOptionalAndroidPermissionsForContentSetting(int contentSettingType) {
        switch (contentSettingType) {
            case ContentSettingsType.GEOLOCATION, ContentSettingsType.GEOLOCATION_WITH_OPTIONS:
                // TODO!
                if (isApproximateLocationSupportEnabled()) {
                    return Arrays.copyOf(
                            LOCATION_OPTIONAL_PERMISSIONS_POST_S,
                            LOCATION_OPTIONAL_PERMISSIONS_POST_S.length);
                }
                return EMPTY_PERMISSIONS;
            default:
                return EMPTY_PERMISSIONS;
        }
    }

    @CalledByNative
    private static boolean doesAppLevelSettingsAllowSiteNotifications() {
        ContextualNotificationPermissionRequester contextualPermissionRequester =
                ContextualNotificationPermissionRequester.getInstance();
        return contextualPermissionRequester != null
                && contextualPermissionRequester.doesAppLevelSettingsAllowSiteNotifications();
    }

    public static boolean hasSystemPermissionsForBluetooth(WindowAndroid windowAndroid) {
        return !needsNearbyDevicesPermissionForBluetooth(windowAndroid)
                && !needsLocationPermissionForBluetooth(windowAndroid);
    }

    @CalledByNative
    public static boolean canRequestSystemPermission(
            int contentSettingType, WindowAndroid windowAndroid) {
        String[] permissions = getRequiredAndroidPermissionsForContentSetting(contentSettingType);
        for (String permission : permissions) {
            if (!windowAndroid.canRequestPermission(permission)) {
                return false;
            }
        }
        return true;
    }

    @CalledByNative
    public static boolean needsLocationPermissionForBluetooth(WindowAndroid windowAndroid) {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.S
                && !windowAndroid.hasPermission(Manifest.permission.ACCESS_FINE_LOCATION);
    }

    @CalledByNative
    public static boolean needsNearbyDevicesPermissionForBluetooth(WindowAndroid windowAndroid) {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && (!windowAndroid.hasPermission(Manifest.permission.BLUETOOTH_SCAN)
                        || !windowAndroid.hasPermission(Manifest.permission.BLUETOOTH_CONNECT));
    }

    @CalledByNative
    public static boolean needsLocationServicesForBluetooth() {
        // Location services are not required on Android S+ to use Bluetooth if the application has
        // Nearby Devices permission and has set the neverForLocation flag on the BLUETOOTH_SCAN
        // permission in its manifest.
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.S
                && !LocationUtils.getInstance().isSystemLocationSettingEnabled();
    }

    @CalledByNative
    public static boolean canRequestSystemPermissionsForBluetooth(WindowAndroid windowAndroid) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return windowAndroid.canRequestPermission(Manifest.permission.BLUETOOTH_SCAN)
                    && windowAndroid.canRequestPermission(Manifest.permission.BLUETOOTH_CONNECT);
        }

        return windowAndroid.canRequestPermission(Manifest.permission.ACCESS_FINE_LOCATION);
    }

    @CalledByNative
    public static void requestSystemPermissionsForBluetooth(
            WindowAndroid windowAndroid, PermissionCallback callback) {
        String[] requiredPermissions;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            requiredPermissions =
                    new String[] {
                        Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT
                    };
        } else {
            requiredPermissions = new String[] {Manifest.permission.ACCESS_FINE_LOCATION};
        }
        // TODO(crbug.com/40255210): Removes this checking for null callback.
        if (callback == null) {
            callback = (permissions, grantResults) -> {};
        }
        windowAndroid.requestPermissions(requiredPermissions, callback);
    }

    @CalledByNative
    public static void requestLocationServices(WindowAndroid windowAndroid) {
        assumeNonNull(windowAndroid.getActivity().get())
                .startActivity(LocationUtils.getInstance().getSystemLocationSettingsIntent());
    }

    public static @ContentSettingsType.EnumType int getGeolocationType() {
        boolean enabled =
                PermissionsAndroidFeatureMap.isEnabled(
                        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION);
        return enabled
                ? ContentSettingsType.GEOLOCATION_WITH_OPTIONS
                : ContentSettingsType.GEOLOCATION;
    }

    /**
     * Handles the "Allow" action from a permission prompt (e.g. Message UI). Requests Android
     * permission if needed, then resolves the site permission.
     *
     * @param window The WindowAndroid.
     * @param webContents The WebContents.
     * @param contentSettingsType The ContentSettingsType.
     */
    @CalledByNative
    public static void handlePermissionPromptAllow(
            WindowAndroid window,
            WebContents webContents,
            @ContentSettingsType.EnumType int contentSettingsType) {
        requestAndResolveNotificationsPermissionRequest(
                window,
                webContents,
                () -> {
                    boolean granted = window.hasPermission(Manifest.permission.POST_NOTIFICATIONS);
                    PermissionDialogController.showLoudClapperDialogResultIcon(
                            window, granted ? ContentSetting.ALLOW : ContentSetting.BLOCK);
                });
    }

    /**
     * If necessary, requests a Android OS level notification permission and resolves the pending
     * request based on the result.
     *
     * <p>This is used by the Clapper Loud code in the "Subscribe" button (PageInfo). If the Android
     * permission was not granted on OS level, this method will asynchronously request the OS level
     * permission using a dialog and run the callback when the user has made a decision. In case the
     * OS level permission was already granted before, it accepts the notifications permission.
     *
     * @param window The WindowAndroid.
     * @param webContents The WebContents.
     * @param onResolved Callback runnable executed when the OS level permission is resolved
     *     (granted or denied).
     */
    public static void requestAndResolveNotificationsPermissionRequest(
            WindowAndroid window, WebContents webContents, Runnable onResolved) {
        // Either returns directly in case the OS level notifications permission was already
        // granted or asks for it asynchronously before granting/denying the request.
        boolean requestSent =
                AndroidPermissionRequester.requestAndroidPermissions(
                        window,
                        new int[] {ContentSettingsType.NOTIFICATIONS},
                        new AndroidPermissionRequester.RequestDelegate() {
                            @Override
                            public void onAndroidPermissionAccepted() {
                                RecordHistogram.recordBooleanHistogram(
                                        "Permissions.ClapperLoud.PageInfo.OsPromptResolved", true);
                                PermissionUtilJni.get()
                                        .resolveNotificationsPermissionRequest(
                                                webContents, ContentSetting.ALLOW);
                                onResolved.run();
                            }

                            @Override
                            public void onAndroidPermissionCanceled() {
                                RecordHistogram.recordBooleanHistogram(
                                        "Permissions.ClapperLoud.PageInfo.OsPromptResolved", false);
                                PermissionUtilJni.get()
                                        .dismissNotificationsPermissionRequest(webContents);
                                onResolved.run();
                            }
                        });
        // The OS level permission for notifications was already granted; therefore, the
        // permission request can be allowed.
        if (!requestSent) {
            PermissionUtilJni.get()
                    .resolveNotificationsPermissionRequest(webContents, ContentSetting.ALLOW);
            onResolved.run();
        }
    }

    /**
     * Grants a notifications permission if it is requested.
     *
     * <p>This method is called when the user clicks on the "Subscribe" button in the notifications
     * permission row in PageInfo.
     */
    public static void resolveNotificationsPermissionRequest(
            WebContents webContents, @ContentSetting int contentSetting) {
        PermissionUtilJni.get().resolveNotificationsPermissionRequest(webContents, contentSetting);
    }

    /**
     * Dismisses a notifications permission request.
     *
     * <p>This method is called when the user clicks on the "Subscribe" button in the notifications
     * permission row in PageInfo but did not grant the Android OS level permission prompt. Despite
     * the user granted the site-level permission, we still need to dismiss the permission request
     * as Chrome doesn't have the Android OS level permission and hence the permission request is no
     * longer valid.
     */
    public static void dismissNotificationsPermissionRequest(WebContents webContents) {
        PermissionUtilJni.get().dismissNotificationsPermissionRequest(webContents);
    }

    @NativeMethods
    public interface Natives {
        void resolveNotificationsPermissionRequest(
                WebContents webContents, @ContentSetting int contentSetting);

        void dismissNotificationsPermissionRequest(WebContents webContents);

        void notifyQuietIconDismissed(WebContents webContents);
    }

    /**
     * Notifies the native side that the quiet permission icon has been dismissed (e.g. by timeout).
     *
     * <p>TODO(crbug.com/463333225): Clean this provisional function name up if Clapper is launched
     * or removed.
     *
     * @param webContents The {@link WebContents} associated with the dismissed icon.
     */
    public static void notifyQuietIconDismissed(WebContents webContents) {
        PermissionUtilJni.get().notifyQuietIconDismissed(webContents);
    }
}
