// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;
import static org.chromium.components.permissions.PermissionUtil.getGeolocationType;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Process;
import android.provider.Settings;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;

import androidx.annotation.IntDef;
import androidx.preference.Preference;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.PermissionUtil;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.subresource_filter.SubresourceFilterFeatureMap;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A base class for dealing with website settings categories. */
@NullMarked
public class SiteSettingsCategory {
    @IntDef({
        Type.ALL_SITES,
        Type.ADS,
        Type.AUGMENTED_REALITY,
        Type.AUTOMATIC_DOWNLOADS,
        Type.BACKGROUND_SYNC,
        Type.BLUETOOTH,
        Type.BLUETOOTH_SCANNING,
        Type.CAMERA,
        Type.CLIPBOARD,
        Type.HAND_TRACKING,
        Type.IDLE_DETECTION,
        Type.DEVICE_LOCATION,
        Type.JAVASCRIPT,
        Type.MICROPHONE,
        Type.NFC,
        Type.NOTIFICATIONS,
        Type.POPUPS,
        Type.PROTECTED_MEDIA,
        Type.SENSORS,
        Type.SOUND,
        Type.USB,
        Type.VIRTUAL_REALITY,
        Type.USE_STORAGE,
        Type.AUTO_DARK_WEB_CONTENT,
        Type.REQUEST_DESKTOP_SITE,
        Type.FEDERATED_IDENTITY_API,
        Type.THIRD_PARTY_COOKIES,
        Type.SITE_DATA,
        Type.ANTI_ABUSE,
        Type.ZOOM,
        Type.STORAGE_ACCESS,
        Type.TRACKING_PROTECTION,
        Type.FILE_EDITING,
        Type.JAVASCRIPT_OPTIMIZER,
        Type.SERIAL_PORT,
        Type.LOCAL_NETWORK_ACCESS,
        Type.WINDOW_MANAGEMENT,
        Type.AUTO_PICTURE_IN_PICTURE,
        Type.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        // All updates here must also be reflected in {@link #preferenceKey(int)
        // preferenceKey} and {@link #contentSettingsType(int) contentSettingsType}.
        int ALL_SITES = 0;
        int ADS = 1;
        int AUGMENTED_REALITY = 2;
        int AUTOMATIC_DOWNLOADS = 3;
        int BACKGROUND_SYNC = 4;
        int BLUETOOTH_SCANNING = 5;
        int CAMERA = 6;
        int CLIPBOARD = 7;
        int DEVICE_LOCATION = 8;
        int IDLE_DETECTION = 9;
        int JAVASCRIPT = 10;
        int MICROPHONE = 11;
        int NFC = 12;
        int NOTIFICATIONS = 13;
        int POPUPS = 14;
        int PROTECTED_MEDIA = 15;
        int SENSORS = 16;
        int SOUND = 17;
        int USB = 18;
        int BLUETOOTH = 19;
        int VIRTUAL_REALITY = 20;
        int USE_STORAGE = 21;
        int AUTO_DARK_WEB_CONTENT = 22;
        int REQUEST_DESKTOP_SITE = 23;
        int FEDERATED_IDENTITY_API = 24;
        int THIRD_PARTY_COOKIES = 25;
        int SITE_DATA = 26;
        int ANTI_ABUSE = 27;
        int ZOOM = 28;
        int STORAGE_ACCESS = 29;
        int TRACKING_PROTECTION = 30;
        int HAND_TRACKING = 31;
        int FILE_EDITING = 32;
        int JAVASCRIPT_OPTIMIZER = 33;
        int SERIAL_PORT = 34;
        int LOCAL_NETWORK_ACCESS = 35;
        int WINDOW_MANAGEMENT = 36;
        int AUTO_PICTURE_IN_PICTURE = 37;

        /** Number of handled categories used for calculating array sizes. */
        int NUM_ENTRIES = 38;
    }

    private final BrowserContextHandle mBrowserContextHandle;

    // The id of this category.
    private final @Type int mCategory;

    // The id of a permission in Android M that governs this category. Can be blank if Android has
    // no equivalent permission for the category.
    private final String mAndroidPermission;

    /**
     * Construct a SiteSettingsCategory.
     *
     * @param category The string id of the category to construct.
     * @param androidPermission A string containing the id of a toggle-able permission in Android
     *     that this category represents (or blank, if Android does not expose that permission).
     */
    protected SiteSettingsCategory(
            BrowserContextHandle browserContextHandle,
            @Type int category,
            String androidPermission) {
        mBrowserContextHandle = browserContextHandle;
        mCategory = category;
        mAndroidPermission = androidPermission;
    }

    /** Construct a SiteSettingsCategory for type DEVICE_LOCATION. */
    public static SiteSettingsCategory createForDeviceLocation(
            BrowserContextHandle browserContextHandle, boolean forPreciseLocation) {
        return new LocationCategory(browserContextHandle, forPreciseLocation);
    }

    /** Construct a SiteSettingsCategory from a type. */
    public static SiteSettingsCategory createFromType(
            BrowserContextHandle browserContextHandle, @Type int type) {
        if (type == Type.DEVICE_LOCATION) {
            return new LocationCategory(browserContextHandle, /* forPreciseLocation= */ true);
        }
        if (type == Type.NFC) return new NfcCategory(browserContextHandle);
        if (type == Type.NOTIFICATIONS) return new NotificationCategory(browserContextHandle);
        if (type == Type.JAVASCRIPT_OPTIMIZER) {
            return new JavascriptOptimizerCategory(browserContextHandle);
        }

        final String permission;
        if (type == Type.CAMERA) {
            permission = android.Manifest.permission.CAMERA;
        } else if (type == Type.MICROPHONE) {
            permission = android.Manifest.permission.RECORD_AUDIO;
        } else if (type == Type.AUGMENTED_REALITY) {
            permission = android.Manifest.permission.CAMERA;
        } else if (type == Type.HAND_TRACKING
                && PermissionUtil.handTrackingNeedsAdditionalPermissions()) {
            permission = PermissionUtil.ANDROID_PERMISSION_HAND_TRACKING;
        } else {
            permission = "";
        }
        return new SiteSettingsCategory(browserContextHandle, type, permission);
    }

    public static @Nullable SiteSettingsCategory createFromContentSettingsType(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType.EnumType int contentSettingsType) {
        assert contentSettingsType != -1;
        assert Type.ALL_SITES == 0;
        for (@Type int i = Type.ALL_SITES; i < Type.NUM_ENTRIES; i++) {
            if (contentSettingsType(i) == contentSettingsType) {
                return createFromType(browserContextHandle, i);
            }
        }
        return null;
    }

    public static @Nullable SiteSettingsCategory createFromPreferenceKey(
            BrowserContextHandle browserContextHandle, String preferenceKey) {
        assert Type.ALL_SITES == 0;
        for (@Type int i = Type.ALL_SITES; i < Type.NUM_ENTRIES; i++) {
            if (preferenceKey(i).equals(preferenceKey)) {
                return createFromType(browserContextHandle, i);
            }
        }
        return null;
    }

    /** Convert Type into {@link ContentSettingsType}. */
    public static @ContentSettingsType.EnumType int contentSettingsType(@Type int type) {
        // This switch statement is ordered by types alphabetically.
        switch (type) {
            case Type.ADS:
                return ContentSettingsType.ADS;
            case Type.ANTI_ABUSE:
                return ContentSettingsType.ANTI_ABUSE;
            case Type.AUGMENTED_REALITY:
                return ContentSettingsType.AR;
            case Type.AUTO_DARK_WEB_CONTENT:
                return ContentSettingsType.AUTO_DARK_WEB_CONTENT;
            case Type.AUTO_PICTURE_IN_PICTURE:
                return ContentSettingsType.AUTO_PICTURE_IN_PICTURE;
            case Type.AUTOMATIC_DOWNLOADS:
                return ContentSettingsType.AUTOMATIC_DOWNLOADS;
            case Type.BACKGROUND_SYNC:
                return ContentSettingsType.BACKGROUND_SYNC;
            case Type.BLUETOOTH:
                return ContentSettingsType.BLUETOOTH_GUARD;
            case Type.BLUETOOTH_SCANNING:
                return ContentSettingsType.BLUETOOTH_SCANNING;
            case Type.CAMERA:
                return ContentSettingsType.MEDIASTREAM_CAMERA;
            case Type.CLIPBOARD:
                return ContentSettingsType.CLIPBOARD_READ_WRITE;
            case Type.SITE_DATA:
            case Type.THIRD_PARTY_COOKIES:
                return ContentSettingsType.COOKIES;
            case Type.REQUEST_DESKTOP_SITE:
                return ContentSettingsType.REQUEST_DESKTOP_SITE;
            case Type.DEVICE_LOCATION:
                return getGeolocationType();
            case Type.FILE_EDITING:
                return ContentSettingsType.FILE_SYSTEM_WRITE_GUARD;
            case Type.FEDERATED_IDENTITY_API:
                return ContentSettingsType.FEDERATED_IDENTITY_API;
            case Type.HAND_TRACKING:
                return ContentSettingsType.HAND_TRACKING;
            case Type.IDLE_DETECTION:
                return ContentSettingsType.IDLE_DETECTION;
            case Type.JAVASCRIPT:
                return ContentSettingsType.JAVASCRIPT;
            case Type.JAVASCRIPT_OPTIMIZER:
                return ContentSettingsType.JAVASCRIPT_OPTIMIZER;
            case Type.LOCAL_NETWORK_ACCESS:
                return ContentSettingsType.LOCAL_NETWORK_ACCESS;
            case Type.MICROPHONE:
                return ContentSettingsType.MEDIASTREAM_MIC;
            case Type.NFC:
                return ContentSettingsType.NFC;
            case Type.NOTIFICATIONS:
                return ContentSettingsType.NOTIFICATIONS;
            case Type.POPUPS:
                return ContentSettingsType.POPUPS;
            case Type.PROTECTED_MEDIA:
                return ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER;
            case Type.SENSORS:
                return ContentSettingsType.SENSORS;
            case Type.SERIAL_PORT:
                return ContentSettingsType.SERIAL_GUARD;
            case Type.STORAGE_ACCESS:
                return ContentSettingsType.STORAGE_ACCESS;
            case Type.SOUND:
                return ContentSettingsType.SOUND;
            case Type.USB:
                return ContentSettingsType.USB_GUARD;
            case Type.VIRTUAL_REALITY:
                return ContentSettingsType.VR;
            case Type.WINDOW_MANAGEMENT:
                return ContentSettingsType.WINDOW_MANAGEMENT;
            case Type.ALL_SITES:
            case Type.USE_STORAGE:
            case Type.ZOOM:
            case Type.TRACKING_PROTECTION:
                return ContentSettingsType.DEFAULT; // Conversion unavailable.
        }
        assert false;
        return ContentSettingsType.DEFAULT;
    }

    /**
     * Get the chooser data type {@link ContentSettingsType} corresponding to the given {@link
     * ContentSettingsType}.
     */
    public static int objectChooserDataTypeFromGuard(@ContentSettingsType.EnumType int type) {
        switch (type) {
            case ContentSettingsType.USB_GUARD:
                return ContentSettingsType.USB_CHOOSER_DATA;
            case ContentSettingsType.BLUETOOTH_GUARD:
                return ContentSettingsType.BLUETOOTH_CHOOSER_DATA;
            case ContentSettingsType.SERIAL_GUARD:
                return ContentSettingsType.SERIAL_CHOOSER_DATA;
            default:
                return -1; // Conversion unavailable.
        }
    }

    /** Convert Type into preference String */
    public static String preferenceKey(@Type int type) {
        // This switch statement is ordered by types alphabetically.
        switch (type) {
            case Type.ADS:
                return "ads";
            case Type.ANTI_ABUSE:
                return "anti_abuse";
            case Type.AUGMENTED_REALITY:
                return "augmented_reality";
            case Type.AUTO_DARK_WEB_CONTENT:
                return "auto_dark_web_content";
            case Type.AUTO_PICTURE_IN_PICTURE:
                return "auto_picture_in_picture";
            case Type.ALL_SITES:
                return "all_sites";
            case Type.AUTOMATIC_DOWNLOADS:
                return "automatic_downloads";
            case Type.BACKGROUND_SYNC:
                return "background_sync";
            case Type.BLUETOOTH:
                return "bluetooth";
            case Type.BLUETOOTH_SCANNING:
                return "bluetooth_scanning";
            case Type.CAMERA:
                return "camera";
            case Type.CLIPBOARD:
                return "clipboard";
            case Type.REQUEST_DESKTOP_SITE:
                return "request_desktop_site";
            case Type.DEVICE_LOCATION:
                return "device_location";
            case Type.FEDERATED_IDENTITY_API:
                return "federated_identity_api";
            case Type.FILE_EDITING:
                return "file_editing";
            case Type.HAND_TRACKING:
                return "hand_tracking";
            case Type.IDLE_DETECTION:
                return "idle_detection";
            case Type.JAVASCRIPT:
                return "javascript";
            case Type.JAVASCRIPT_OPTIMIZER:
                return "javascript_optimizer";
            case Type.LOCAL_NETWORK_ACCESS:
                return "local_network_access";
            case Type.MICROPHONE:
                return "microphone";
            case Type.NFC:
                return "nfc";
            case Type.NOTIFICATIONS:
                return "notifications";
            case Type.POPUPS:
                return "popups";
            case Type.PROTECTED_MEDIA:
                return "protected_content";
            case Type.SENSORS:
                return "sensors";
            case Type.SERIAL_PORT:
                return "serial_port";
            case Type.STORAGE_ACCESS:
                return "storage_access";
            case Type.SOUND:
                return "sound";
            case Type.USB:
                return "usb";
            case Type.USE_STORAGE:
                return "use_storage";
            case Type.VIRTUAL_REALITY:
                return "virtual_reality";
            case Type.SITE_DATA:
                return "site_data";
            case Type.THIRD_PARTY_COOKIES:
                return "third_party_cookies";
            case Type.TRACKING_PROTECTION:
                return "tracking_protection";
            case Type.WINDOW_MANAGEMENT:
                return "window_management";
            case Type.ZOOM:
                return "zoom";
            default:
                assert false;
                return "";
        }
    }

    /** Returns the {@link SiteSettingsCategory.Type} for this category. */
    public @Type int getType() {
        return mCategory;
    }

    /** Returns the {@link ContentSettingsType} for this category, or -1 if no such type exists. */
    public @ContentSettingsType.EnumType int getContentSettingsType() {
        return contentSettingsType(mCategory);
    }

    /**
     * Returns the {@link ContentSettingsType} representing the chooser data type for this category,
     * or -1 if this category does not have a chooser data type.
     */
    public @ContentSettingsType.EnumType int getObjectChooserDataType() {
        return objectChooserDataTypeFromGuard(contentSettingsType(mCategory));
    }

    /** Returns whether the Ads category is enabled via an experiment flag. */
    public static boolean adsCategoryEnabled() {
        return SubresourceFilterFeatureMap.isSubresourceFilterEnabled();
    }

    /**
     * Returns whether the current category is managed either by enterprise policy or by the
     * custodian of a supervised account.
     */
    public boolean isManaged() {
        // TODO(dullweber): Why do we check some permissions for managed state and some for user
        // modifiability and some not at all?
        if (mCategory == Type.AUTOMATIC_DOWNLOADS
                || mCategory == Type.BACKGROUND_SYNC
                || mCategory == Type.JAVASCRIPT
                || mCategory == Type.POPUPS
                || mCategory == Type.JAVASCRIPT_OPTIMIZER) {
            return WebsitePreferenceBridge.isContentSettingManaged(
                    getBrowserContextHandle(), getContentSettingsType());
        } else if (mCategory == Type.DEVICE_LOCATION
                || mCategory == Type.CAMERA
                || mCategory == Type.MICROPHONE) {
            return !WebsitePreferenceBridge.isContentSettingUserModifiable(
                    getBrowserContextHandle(), getContentSettingsType());
        } else if (mCategory == Type.THIRD_PARTY_COOKIES) {
            PrefService prefService = UserPrefs.get(getBrowserContextHandle());
            return prefService.isManagedPreference(COOKIE_CONTROLS_MODE);
        }
        return false;
    }

    /**
     * Returns whether the current category is managed by the custodian (e.g. parent, not an
     * enterprise admin) of the account if the account is supervised.
     */
    public boolean isManagedByCustodian() {
        // TODO(dullweber): Why do we only check these types?
        if (mCategory == Type.DEVICE_LOCATION
                || mCategory == Type.CAMERA
                || mCategory == Type.MICROPHONE
                || mCategory == Type.SITE_DATA) {
            return WebsitePreferenceBridge.isContentSettingManagedByCustodian(
                    getBrowserContextHandle(), getContentSettingsType());
        }
        return false;
    }

    /**
     * Configure a preference to show when when the Android permission for this category is
     * disabled.
     *
     * @param osWarning A preference to hold the first permission warning. After calling this
     *     method, if osWarning has no title, the preference should not be added to the preference
     *     screen.
     * @param osWarningExtra A preference to hold any additional permission warning (if any). After
     *     calling this method, if osWarningExtra has no title, the preference should not be added
     *     to the preference screen.
     * @param context The current context.
     * @param appName The name of the app to use in warning strings.
     */
    public void configureWarningPreferences(
            Preference osWarning, Preference osWarningExtra, Context context, String appName) {
        assert showPermissionBlockedMessage(context);

        Intent globalIntent = getIntentToEnableOsGlobalPermission(context);
        String globalMessage = getMessageForEnablingOsGlobalPermission(context);
        String unsupportedMessage = getMessageIfNotSupported(context);

        int color = SemanticColorUtils.getDefaultControlColorActive(context);
        ForegroundColorSpan linkSpan = new ForegroundColorSpan(color);

        boolean showPerAppWarning = shouldShowPerAppWarning(context);
        if (showPerAppWarning) {
            Intent perAppIntent = getAppInfoIntent(context);
            String perAppMessage = getMessageForEnablingOsPerAppPermission(context, appName);
            SpannableString messageWithLink =
                    SpanApplier.applySpans(
                            perAppMessage, new SpanInfo("<link>", "</link>", linkSpan));
            osWarning.setTitle(messageWithLink);
            osWarning.setIntent(perAppIntent);

            osWarning.setIcon(getDisabledInAndroidIcon(context));
        }

        if (!supportedGlobally()) {
            osWarningExtra.setTitle(unsupportedMessage);
            osWarningExtra.setIcon(getDisabledInAndroidIcon(context));
        } else if (globalIntent != null) {
            assumeNonNull(globalMessage);
            SpannableString messageWithLink =
                    SpanApplier.applySpans(
                            globalMessage, new SpanInfo("<link>", "</link>", linkSpan));
            osWarningExtra.setTitle(messageWithLink);
            osWarningExtra.setIntent(globalIntent);

            if (!showPerAppWarning) {
                osWarningExtra.setIcon(getDisabledInAndroidIcon(context));
            } else {
                Drawable transparent = new ColorDrawable(Color.TRANSPARENT);
                osWarningExtra.setIcon(transparent);
            }
        }
    }

    /** Returns the icon for permissions that have been disabled by Android. */
    Drawable getDisabledInAndroidIcon(Context context) {
        Drawable icon =
                ApiCompatibilityUtils.getDrawable(
                        context.getResources(), R.drawable.exclamation_triangle);
        icon.mutate();
        int disabledColor = SemanticColorUtils.getDefaultControlColorActive(context);
        icon.setColorFilter(disabledColor, PorterDuff.Mode.SRC_IN);
        return icon;
    }

    /** Returns the BrowserContextHandle we're showing the Site Settings UI for. */
    protected BrowserContextHandle getBrowserContextHandle() {
        return mBrowserContextHandle;
    }

    /**
     * Returns whether the permission is supported on this device. Some permissions like NFC are
     * backed up by hardware support and may not be available.
     */
    protected boolean supportedGlobally() {
        return true;
    }

    /** Returns the message to display when permission is not supported. */
    protected @Nullable String getMessageIfNotSupported(Context context) {
        return null;
    }

    /**
     * Returns whether the permission is enabled in Android, both globally and per-app. If the
     * permission does not have a per-app setting or a global setting, true is assumed for either
     * that is missing (or both).
     */
    public boolean enabledInAndroid(Context context) {
        return enabledGlobally() && enabledForChrome(context);
    }

    /**
     * Returns whether a permission is enabled across Android. Not all permissions can be disabled
     * globally, so the default is true, but can be overwritten in sub-classes.
     */
    protected boolean enabledGlobally() {
        return true;
    }

    /** Returns whether a permission is enabled for Chrome specifically. */
    protected boolean enabledForChrome(Context context) {
        if (mAndroidPermission.isEmpty()) return true;
        return permissionOnInAndroid(mAndroidPermission, context);
    }

    /** Returns whether to disable the category toggle. */
    protected boolean shouldDisableToggle() {
        return false;
    }

    /**
     * Returns resource id for message about why adding exceptions is blocked. 0 should be returned
     * if no message should be shown.
     */
    protected int getBlockAddingExceptionsReasonResourceId() {
        return 0;
    }

    /**
     * Returns whether to show the 'permission blocked' message. Majority of the time, that is
     * warranted when the permission is either blocked per app or globally. But there are exceptions
     * to this, so the sub-classes can overwrite.
     */
    boolean showPermissionBlockedMessage(Context context) {
        return !enabledForChrome(context) || !enabledGlobally();
    }

    /**
     * Returns whether to show the warning to enable permissions per app, that is when the
     * permission is blocked per app.
     */
    protected boolean shouldShowPerAppWarning(Context context) {
        return !enabledForChrome(context);
    }

    /**
     * Returns the OS Intent to use to enable a permission globally, or null if there is no global
     * permission. Android M and above provides two ways of doing this for some permissions, most
     * notably Location, one that is per-app and another that is global.
     */
    protected @Nullable Intent getIntentToEnableOsGlobalPermission(Context context) {
        return null;
    }

    /**
     * Returns the message to display when per-app permission is blocked.
     */
    protected String getMessageForEnablingOsPerAppPermission(Context context, String appName) {
        @ContentSettingsType.EnumType int type = this.getContentSettingsType();
        int permission_string = R.string.android_permission_off;
        if (type == ContentSettingsType.GEOLOCATION
                || type == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
            permission_string = R.string.android_location_permission_off;
        } else if (type == ContentSettingsType.MEDIASTREAM_MIC) {
            permission_string = R.string.android_microphone_permission_off;
        } else if (type == ContentSettingsType.MEDIASTREAM_CAMERA) {
            permission_string = R.string.android_camera_permission_off;
        } else if (type == ContentSettingsType.AR) {
            permission_string = R.string.android_ar_camera_permission_off;
        } else if (type == ContentSettingsType.HAND_TRACKING) {
            permission_string = R.string.android_hand_tracking_permission_off;
        } else if (type == ContentSettingsType.NOTIFICATIONS) {
            permission_string = R.string.android_notifications_permission_off;
        }
        return context.getString(permission_string, appName);
    }

    /** Returns the message to display when per-app permission is blocked. */
    protected @Nullable String getMessageForEnablingOsGlobalPermission(Context context) {
        return null;
    }

    /**
     * Returns the message to display to explain why the settings toggle is disabled. Returns null
     * if no message should be displayed.
     */
    protected @Nullable String getMessageWhyToggleIsDisabled(Context context) {
        return null;
    }

    /** Returns an Intent to show the App Info page for the current app. */
    protected Intent getAppInfoIntent(Context context) {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.setData(
                new Uri.Builder().scheme("package").opaquePart(context.getPackageName()).build());
        return intent;
    }

    /**
     * Returns whether a per-app permission is enabled.
     *
     * @param permission The string of the permission to check.
     */
    protected boolean permissionOnInAndroid(String permission, Context context) {
        return PackageManager.PERMISSION_GRANTED
                == ApiCompatibilityUtils.checkPermission(
                        context, permission, Process.myPid(), Process.myUid());
    }

    /**
     * A SiteSettingsCategory for a generic OS-level permission warning. This is used when a site
     * has multiple permissions that are blocked by the OS, so a single specific message is not
     * appropriate.
     */
    static class GenericSiteSettingsCategory extends SiteSettingsCategory {
        public GenericSiteSettingsCategory(BrowserContextHandle browserContextHandle) {
            super(browserContextHandle, Type.ALL_SITES, "");
        }

        @Override
        boolean showPermissionBlockedMessage(Context context) {
            return true;
        }

        @Override
        protected String getMessageForEnablingOsPerAppPermission(Context context, String appName) {
            return context.getString(R.string.android_permission_off_plural, appName);
        }

        @Override
        protected boolean shouldShowPerAppWarning(Context context) {
            return true;
        }
    }
}
