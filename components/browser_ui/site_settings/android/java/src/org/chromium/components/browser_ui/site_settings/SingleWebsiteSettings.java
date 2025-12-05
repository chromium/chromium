// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.components.browser_ui.settings.ChromeButtonPreference;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.components.embedder_support.util.ExtensionUrlUtil;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.permissions.PermissionsAndroidFeatureMap;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Shows the permissions and other settings for a particular website. */
@NullMarked
public class SingleWebsiteSettings extends BaseSiteSettingsFragment
        implements EmbeddableSettingsPage,
                Preference.OnPreferenceChangeListener,
                Preference.OnPreferenceClickListener,
                CustomDividerFragment {
    /** Interface for a class that wants to receive updates from SingleWebsiteSettings. */
    public interface Observer {
        /** Notifies the observer that the website was reset. */
        void onPermissionsReset();

        /** Notifies the observer that a permission was changed. */
        void onPermissionChanged();

        /** Notifies the observer that the location permission subpage button was clicked. */
        void onLocationPermissionSubpageClicked();

        /** Notifies the observer that the notification subscribe button was clicked. */
        void onNotificationSubscribeClicked();
    }

    // SingleWebsiteSettings expects either EXTRA_SITE (a Website) or
    // EXTRA_SITE_ADDRESS (a WebsiteAddress) to be present (but not both). If
    // EXTRA_SITE is present, the fragment will display the permissions in that
    // Website object. If EXTRA_SITE_ADDRESS is present, the fragment will find all
    // permissions for that website address and display those.
    public static final String EXTRA_SITE = "org.chromium.chrome.preferences.site";
    public static final String EXTRA_SITE_ADDRESS = "org.chromium.chrome.preferences.site_address";

    // A boolean to configure whether the sound setting should be shown. Defaults to true.
    public static final String EXTRA_SHOW_SOUND = "org.chromium.chrome.preferences.show_sound";

    // A boolean to configure whether the automatic picture in picture setting should be shown.
    // Defaults to true.
    public static final String EXTRA_SHOW_AUTO_PIP =
            "org.chromium.chrome.preferences.show_auto_pip";

    // A boolean that indicates whether these settings were opened from GroupedWebsiteSettings.
    public static final String EXTRA_FROM_GROUPED = "org.chromium.chrome.preferences.from_grouped";

    // Used to store mPreviousNotificationPermission when the activity is paused.
    private static final String PREVIOUS_NOTIFICATION_PERMISSION_KEY =
            "previous_notification_permission";

    // Preference keys, see single_website_preferences.xml
    // Headings:
    public static final String PREF_PAGE_DESCRIPTION = "page_description";
    public static final String PREF_SITE_HEADING = "site_heading";
    public static final String PREF_SITE_TITLE = "site_title";
    public static final String PREF_USAGE = "site_usage";
    public static final String PREF_RELATED_SITES_HEADER = "related_sites_header";
    public static final String PREF_RELATED_SITES = "related_sites";
    public static final String PREF_PERMISSIONS_HEADER = "site_permissions";
    public static final String PREF_OS_PERMISSIONS_WARNING = "os_permissions_warning";
    public static final String PREF_OS_PERMISSIONS_WARNING_EXTRA = "os_permissions_warning_extra";
    public static final String PREF_OS_PERMISSIONS_WARNING_DIVIDER =
            "os_permissions_warning_divider";
    public static final String PREF_FILE_EDITING_GRANTS = "file_editing_grants";
    public static final String PREF_INTRUSIVE_ADS_INFO = "intrusive_ads_info";
    public static final String PREF_INTRUSIVE_ADS_INFO_DIVIDER = "intrusive_ads_info_divider";
    // Actions at the top (if adding new, see hasUsagePreferences below):
    public static final String PREF_CLEAR_DATA = "clear_data";
    // Buttons:
    public static final String PREF_RESET_SITE = "reset_site_button";

    public static final int REQUEST_CODE_NOTIFICATION_CHANNEL_SETTINGS = 1;

    private static boolean arrayContains(int[] array, int element) {
        for (int e : array) {
            if (e == element) {
                return true;
            }
        }
        return false;
    }

    /**
     * @param type ContentSettingsType
     * @return The preference key of this type
     */
    @VisibleForTesting
    public static @Nullable String getPreferenceKey(@ContentSettingsType.EnumType int type) {
        switch (type) {
            case ContentSettingsType.ADS:
                return "ads_permission_list";
            case ContentSettingsType.AUTO_DARK_WEB_CONTENT:
                return "auto_dark_web_content_permission_list";
            case ContentSettingsType.AUTO_PICTURE_IN_PICTURE:
                return "auto_picture_in_picture_permission_list";
            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
                return "automatic_downloads_permission_list";
            case ContentSettingsType.BACKGROUND_SYNC:
                return "background_sync_permission_list";
            case ContentSettingsType.BLUETOOTH_SCANNING:
                return "bluetooth_scanning_permission_list";
            case ContentSettingsType.COOKIES:
                return "cookies_permission_list";
            case ContentSettingsType.FEDERATED_IDENTITY_API:
                return "federated_identity_api_list";
            case ContentSettingsType.IDLE_DETECTION:
                return "idle_detection_permission_list";
            case ContentSettingsType.JAVASCRIPT:
                return "javascript_permission_list";
            case ContentSettingsType.JAVASCRIPT_OPTIMIZER:
                return "javascript_optimizer";
            case ContentSettingsType.POPUPS:
                return "popup_permission_list";
            case ContentSettingsType.SOUND:
                return "sound_permission_list";
            case ContentSettingsType.AR:
                return "ar_permission_list";
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                return "camera_permission_list";
            case ContentSettingsType.GEOLOCATION:
                return "location_access_list";
            case ContentSettingsType.GEOLOCATION_WITH_OPTIONS:
                return "location_with_options_access_list";
            case ContentSettingsType.HAND_TRACKING:
                return "hand_tracking_permission_list";
            case ContentSettingsType.MEDIASTREAM_MIC:
                return "microphone_permission_list";
            case ContentSettingsType.MIDI_SYSEX:
                return "midi_sysex_permission_list";
            case ContentSettingsType.NFC:
                return "nfc_permission_list";
            case ContentSettingsType.NOTIFICATIONS:
                return "push_notifications_list";
            case ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER:
                return "protected_media_identifier_permission_list";
            case ContentSettingsType.REQUEST_DESKTOP_SITE:
                return "request_desktop_site_permission_list";
            case ContentSettingsType.SENSORS:
                return "sensors_permission_list";
            case ContentSettingsType.VR:
                return "vr_permission_list";
            case ContentSettingsType.CLIPBOARD_READ_WRITE:
                return "clipboard_permission_list";
            case ContentSettingsType.FILE_SYSTEM_WRITE_GUARD:
                return "file_system_write_guard_permission_list";
            case ContentSettingsType.LOCAL_NETWORK_ACCESS:
                return "local_network_access";
            case ContentSettingsType.WINDOW_MANAGEMENT:
                return "window_management_permission_list";
            default:
                return null;
        }
    }

    /**
     * @param type ContentSettingsType
     * @return The enabled value of this type (ALLOW or ASK).
     */
    @VisibleForTesting
    public static @ContentSetting int getEnabledValue(int contentType) {
        if (contentType == ContentSettingsType.FILE_SYSTEM_WRITE_GUARD) {
            return ContentSetting.ASK;
        }
        return ContentSetting.ALLOW;
    }

    // A list of preferences keys that will be hidden on this page if this boolean below is true
    private boolean mHideNonPermissionPreferences;
    private static final String[] NON_PERMISSION_PREFERENCES = {
        PREF_SITE_HEADING,
        PREF_SITE_TITLE,
        PREF_USAGE,
        PREF_RELATED_SITES_HEADER,
        PREF_RELATED_SITES,
        PREF_PERMISSIONS_HEADER,
        PREF_CLEAR_DATA,
    };

    /** The permission type to be highlighted on this page, if any. */
    @ContentSettingsType.EnumType private int mHighlightedPermission = ContentSettingsType.DEFAULT;

    /** The highlight color. */
    @ColorRes private int mHighlightColor;

    // The callback to be run after this site is reset.
    private @Nullable Observer mWebsiteSettingsObserver;

    // The website this page is displaying details about.
    private @Nullable Website mSite;

    // Whether these settings were opened from GroupedWebsitesSettings.
    private boolean mFromGrouped;

    private final List<ChromeSwitchPreference> mEmbeddedPermissionPreferences = new ArrayList<>();

    private final List<ChromeImageViewPreference> mChooserPermissionPreferences = new ArrayList<>();

    // Records previous notification permission on Android O+ to allow detection of permission
    // revocation within the Android system permission activity.
    private @ContentSetting @Nullable Integer mPreviousNotificationPermission;

    // Map from preference key to ContentSettingsType.
    private @Nullable Map<String, Integer> mPreferenceMap;

    private @Nullable Dialog mConfirmationDialog;

    // Maximum value used for the order of the permissions
    private int mMaxPermissionOrder;

    // Stores whether the location permission was initially approximate to ensure we toggle between
    // permissions consistently.
    private boolean mHasApproximateLocationGrant;

    // A boolean to configure whether the requested notifications permission should be shown.
    private boolean mHasRequestedNotificationsPermission;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    private class SingleWebsitePermissionsPopulator
            implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        private final WebsiteAddress mSiteAddress;

        private SingleWebsitePermissionsPopulator(WebsiteAddress siteAddress) {
            mSiteAddress = siteAddress;
        }

        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            // This method may be called after the activity has been destroyed.
            // In that case, bail out.
            if (getActivity() == null) return;

            // TODO(mvanouwerkerk): Avoid modifying the outer class from this inner class.
            mSite = mergePermissionAndStorageInfoForTopLevelOrigin(mSiteAddress, sites);

            displaySitePermissions();
        }
    }

    private final Runnable mDataClearedCallback =
            () -> {
                Activity activity = getActivity();
                if (activity == null || activity.isFinishing()) {
                    return;
                }
                removePreferenceSafely(PREF_CLEAR_DATA);
                if (!hasUsagePreferences()) {
                    removePreferenceSafely(PREF_USAGE);
                }
                removeUserChosenObjectPreferences();
                popBackIfNoSettings();
            };

    /**
     * Creates a Bundle with the correct arguments for opening this fragment for the website with
     * the given url.
     *
     * @param url The URL to open the fragment with. This is a complete url including scheme,
     *     domain, port, path, etc.
     * @return The bundle to attach to the preferences intent.
     */
    public static Bundle createFragmentArgsForSite(String url) {
        Bundle fragmentArgs = new Bundle();
        String origin = Origin.createOrThrow(url).toString();
        fragmentArgs.putSerializable(EXTRA_SITE_ADDRESS, WebsiteAddress.create(origin));
        return fragmentArgs;
    }

    /**
     * Creates a Bundle with the correct arguments for opening this fragment for the extension with
     * the given url.
     *
     * @param url The URL to open the fragment with. This is a complete url including scheme,
     *     domain, port, path, etc.
     * @return The bundle to attach to the preferences intent.
     */
    public static Bundle createFragmentArgsForExtensionSite(String url) {
        Bundle fragmentArgs = new Bundle();
        String origin = ExtensionUrlUtil.getOrigin(url);
        fragmentArgs.putSerializable(EXTRA_SITE_ADDRESS, WebsiteAddress.create(origin));
        return fragmentArgs;
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getContext().getString(R.string.prefs_site_settings));

        // Remove this Preference if it gets restored without a valid SiteSettingsDelegate. This
        // can happen e.g. when it is included in PageInfo.
        if (!hasSiteSettingsDelegate()) {
            getParentFragmentManager().beginTransaction().remove(this).commit();
            return;
        }

        Object extraSite = getArguments().getSerializable(EXTRA_SITE);
        Object extraSiteAddress = getArguments().getSerializable(EXTRA_SITE_ADDRESS);

        if (extraSite != null && extraSiteAddress == null) {
            mSite = (Website) extraSite;
            displaySitePermissions();
        } else if (extraSiteAddress != null && extraSite == null) {
            WebsitePermissionsFetcher fetcher =
                    new WebsitePermissionsFetcher(getSiteSettingsDelegate());
            fetcher.fetchAllPreferences(
                    new SingleWebsitePermissionsPopulator((WebsiteAddress) extraSiteAddress));
        } else {
            assert false : "Exactly one of EXTRA_SITE or EXTRA_SITE_ADDRESS must be provided.";
        }

        mFromGrouped = getArguments().getBoolean(EXTRA_FROM_GROUPED, false);
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        setDivider(null);

        getListView().setItemAnimator(null);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        if (mPreviousNotificationPermission != null) {
            outState.putInt(PREVIOUS_NOTIFICATION_PERMISSION_KEY, mPreviousNotificationPermission);
        }
        super.onSaveInstanceState(outState);
    }

    @Override
    public void onViewStateRestored(@Nullable Bundle savedInstanceState) {
        super.onViewStateRestored(savedInstanceState);

        if (savedInstanceState == null) return;

        if (savedInstanceState.containsKey(PREVIOUS_NOTIFICATION_PERMISSION_KEY)) {
            mPreviousNotificationPermission =
                    savedInstanceState.getInt(PREVIOUS_NOTIFICATION_PERMISSION_KEY);
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (mConfirmationDialog != null) {
            mConfirmationDialog.dismiss();
        }
    }

    @Override
    public boolean hasDivider() {
        return false;
    }

    @Override
    public void onDisplayPreferenceDialog(Preference preference) {
        if (preference instanceof ClearWebsiteStorage) {
            // If the activity is getting destroyed or saved, it is not allowed to modify fragments.
            if (assumeNonNull(getFragmentManager()).isStateSaved()) {
                return;
            }
            Callback<Boolean> onDialogClosed =
                    (Boolean confirmed) -> {
                        if (confirmed) {
                            RecordHistogram.recordEnumeratedHistogram(
                                    "Privacy.DeleteBrowsingData.Action",
                                    DeleteBrowsingDataAction.SITES_SETTINGS_PAGE,
                                    DeleteBrowsingDataAction.MAX_VALUE);

                            SiteDataCleaner.clearData(
                                    getSiteSettingsDelegate(),
                                    assumeNonNull(mSite),
                                    mDataClearedCallback);
                        }
                    };
            ClearWebsiteStorageDialog dialogFragment =
                    ClearWebsiteStorageDialog.newInstance(
                            preference, onDialogClosed, /* isGroup= */ false);
            dialogFragment.setTargetFragment(this, 0);
            dialogFragment.show(getFragmentManager(), ClearWebsiteStorageDialog.TAG);
        } else {
            super.onDisplayPreferenceDialog(preference);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        refreshSitePermissions();
    }

    public void refreshSitePermissions() {
        if (mSite != null) {
            displaySitePermissions();
        }
    }

    public void setHideNonPermissionPreferences(boolean hide) {
        mHideNonPermissionPreferences = hide;
    }

    public void setHasRequestedNotificationsPermission(
            boolean hasRequestedNotificationsPermission) {
        mHasRequestedNotificationsPermission = hasRequestedNotificationsPermission;
    }

    public void setWebsiteSettingsObserver(@Nullable Observer observer) {
        mWebsiteSettingsObserver = observer;
    }

    /**
     * Sets the permission row that should be highlighted on the page, with its corresponding color.
     *
     * @param permission The ContentSettingsType for the permission to be highlighted.
     * @param colorResId The color resource id for the background color of the permission row.
     */
    public void setHighlightedPermission(
            @ContentSettingsType.EnumType int permission, @ColorRes int colorResId) {
        mHighlightedPermission = permission;
        mHighlightColor = colorResId;
    }

    /**
     * Given an address and a list of sets of websites, returns a new site with the same origin
     * as |address| which has merged into it the permissions and storage info of the matching input
     * sites. If a permission is found more than once, the one found first is used and the latter
     * are ignored. This should not drop any relevant data as there should not be duplicates like
     * that in the first place.
     *
     * @param address The address to search for.
     * @param websites The websites to search in.
     * @return The merged website.
     */
    public static Website mergePermissionAndStorageInfoForTopLevelOrigin(
            WebsiteAddress address, Collection<Website> websites) {
        String origin = address.getOrigin();
        String host = assumeNonNull(Uri.parse(origin).getHost());
        String domainAndRegistry = assumeNonNull(address.getDomainAndRegistry());
        Website merged = new Website(address, null);
        // This loop looks expensive, but the amount of data is likely to be relatively small
        // because most sites have very few permissions.
        for (Website other : websites) {
            if (merged.getContentSettingException(ContentSettingsType.ADS) == null
                    && other.getContentSettingException(ContentSettingsType.ADS) != null
                    && other.compareByAddressTo(merged) == 0) {
                merged.setContentSettingException(
                        ContentSettingsType.ADS,
                        other.getContentSettingException(ContentSettingsType.ADS));
            }
            for (PermissionInfo info : other.getPermissionInfos()) {
                if (merged.getPermissionInfo(info.getContentSettingsType()) == null
                        && permissionInfoIsForTopLevelOrigin(info, origin)) {
                    merged.setPermissionInfo(info);
                }
            }
            for (var exceptionList : other.getEmbeddedPermissions().values()) {
                for (var exception : exceptionList) {
                    boolean matchesOrigin =
                            other.getEmbedder() != null
                                    && org.chromium.components.browser_ui.site_settings
                                            .WebsitePreferenceBridgeJni.get()
                                            .urlMatchesContentSettingsPattern(
                                                    origin,
                                                    assumeNonNull(exception.getSecondaryPattern()));
                    if (matchesOrigin) {
                        merged.addEmbeddedPermission(exception);
                    }
                }
            }

            if (merged.getLocalStorageInfo() == null
                    && other.getLocalStorageInfo() != null
                    && origin.equals(other.getLocalStorageInfo().getOrigin())) {
                merged.setLocalStorageInfo(other.getLocalStorageInfo());
            }
            for (StorageInfo storageInfo : other.getStorageInfo()) {
                if (host.equals(storageInfo.getHost())) {
                    merged.addStorageInfo(storageInfo);
                }
            }
            for (SharedDictionaryInfo sharedDictionaryInfo : other.getSharedDictionaryInfo()) {
                if (origin.equals(sharedDictionaryInfo.getOrigin())) {
                    merged.addSharedDictionaryInfo(sharedDictionaryInfo);
                }
            }
            if (merged.getRwsCookieInfo() == null
                    && other.getRwsCookieInfo() != null
                    && domainAndRegistry.equals(other.getAddress().getDomainAndRegistry())) {
                merged.setRwsCookieInfo(other.getRwsCookieInfo());
            }
            if (merged.getFileEditingInfo() == null
                    && other.getFileEditingInfo() != null
                    && origin.equals(other.getFileEditingInfo().getOrigin())) {
                merged.setFileEditingInfo(other.getFileEditingInfo());
            }
            for (ChosenObjectInfo objectInfo : other.getChosenObjectInfo()) {
                if (origin.equals(objectInfo.getOrigin())) {
                    merged.addChosenObjectInfo(objectInfo);
                }
            }
            if (host.equals(other.getAddress().getHost())) {
                for (ContentSettingException exception : other.getContentSettingExceptions()) {
                    int type = exception.getContentSettingType();
                    if (type == ContentSettingsType.ADS) {
                        continue;
                    }
                    if (merged.getContentSettingException(type) == null) {
                        merged.setContentSettingException(type, exception);
                    }
                }
            }

            merged.setDomainImportant(merged.isDomainImportant() || other.isDomainImportant());

            // TODO(crbug.com/40539464): Deal with this TODO colony.
            // TODO(mvanouwerkerk): Make the various info types share a common interface that
            // supports reading the origin or host.
            // TODO(lshang): Merge in CookieException? It will use patterns.
        }
        return merged;
    }

    private static boolean permissionInfoIsForTopLevelOrigin(PermissionInfo info, String origin) {
        // TODO(mvanouwerkerk): Find a more generic place for this method.
        return origin.equals(info.getOrigin())
                && (origin.equals(info.getEmbedderSafe())
                        || SITE_WILDCARD.equals(info.getEmbedderSafe()));
    }

    private @Nullable Drawable getContentSettingsIcon(
            @ContentSettingsType.EnumType int contentSettingsType,
            @ContentSetting @Nullable Integer value) {
        return ContentSettingsResources.getContentSettingsIcon(
                getContext(), contentSettingsType, value);
    }

    /**
     * @return The website this page is displaying details about.
     */
    public @Nullable Website getSite() {
        return mSite;
    }

    /**
     * Updates the permissions displayed in the UI by fetching them from mSite. Must only be called
     * once mSite is set.
     */
    @RequiresNonNull({"mSite"})
    private void displaySitePermissions() {
        if (getPreferenceScreen() != null) {
            getPreferenceScreen().removeAll();
        }
        SettingsUtils.addPreferencesFromResource(this, R.xml.single_website_preferences);

        Preference siteTitlePref = findPreference(PREF_SITE_TITLE);
        siteTitlePref.setTitle(mSite.getTitle());

        SiteSettingsCategory categoryWithWarning = getWarningCategory();

        setupContentSettingsPreferences();
        setUpEmbeddedContentSettingPreferences();
        setUpChosenObjectPreferences();
        setupFileEditingGrants(/* setOrder= */ true);
        setupResetSitePreference();
        setUpClearDataPreference();
        setUpOsWarningPreferences(categoryWithWarning);
        setUpRelatedSitesPreferences();

        setUpAdsInformationalBanner();

        // Remove categories if no sub-items.
        if (!hasUsagePreferences()) {
            removePreferenceSafely(PREF_USAGE);
        }
        if (!hasPermissionsPreferences()) {
            removePreferenceSafely(PREF_PERMISSIONS_HEADER);
        }

        // Remove certain preferences explicitly
        if (mHideNonPermissionPreferences) {
            for (String key : NON_PERMISSION_PREFERENCES) {
                removePreferenceSafely(key);
            }
        } else {
            removePreferenceSafely(PREF_PAGE_DESCRIPTION);
        }
    }

    @RequiresNonNull({"mSite"})
    private void setupContentSettingsPreferences() {
        Preference permissionsHeaderPref = findPreference(PREF_PERMISSIONS_HEADER);
        mMaxPermissionOrder = permissionsHeaderPref.getOrder();
        for (@ContentSettingsType.EnumType int type : SiteSettingsUtil.SETTINGS_ORDER) {
            Preference preference = getPermissionPreference(type);
            preference.setKey(getPreferenceKey(type));

            if (type == ContentSettingsType.ADS) {
                setUpAdsPreference(preference);
            } else if (type == ContentSettingsType.SOUND) {
                setUpSoundPreference(preference);
            } else if (type == ContentSettingsType.JAVASCRIPT) {
                setUpJavascriptPreference(preference);
            } else if (type == ContentSettingsType.GEOLOCATION) {
                setUpLocationPreference(preference);
            } else if (type == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
                setUpLocationWithOptionsPreference(preference);
            } else if (type == ContentSettingsType.NOTIFICATIONS) {
                setUpNotificationsPreference(preference, mSite.isEmbargoed(type));
            } else if (type == ContentSettingsType.AUTO_PICTURE_IN_PICTURE) {
                // On Android, Auto-PiP does not have a prompt, so the UI treats the ASK
                // state as ALLOW in regular mode and BLOCK in incognito. This logic should
                // be removed when a prompt is implemented for parity with desktop.
                setUpAutoPictureInPicturePreference(preference);
            } else {
                setupContentSettingsPreference(
                        preference,
                        mSite.getContentSetting(getBrowserContextHandle(), type),
                        mSite.isEmbargoed(type),
                        isOneTime(type));
            }
        }
    }

    private Preference getPermissionPreference(@ContentSettingsType.EnumType int type) {
        boolean isOneTime = isOneTime(type);
        if (type == ContentSettingsType.GEOLOCATION_WITH_OPTIONS && !isOneTime) {
            return createTwoActionLocationSwitchPreference();
        }

        return (isOneTime
                        && PermissionsAndroidFeatureMap.isEnabled(
                                PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION))
                ? new ChromeImageViewPreference(getStyledContext())
                : new ChromeSwitchPreference(getStyledContext());
    }

    private TwoActionSwitchPreference createTwoActionLocationSwitchPreference() {
        TwoActionSwitchPreference preference = new TwoActionSwitchPreference(getStyledContext());
        preference.setPrimaryButtonClickListener((v) -> openLocationPermissionSubpage());
        return preference;
    }

    private void openLocationPermissionSubpage() {
        if (getSettingsNavigation() != null) {
            Bundle fragmentArgs = new Bundle();
            fragmentArgs.putSerializable(EXTRA_SITE, mSite);
            getSettingsNavigation()
                    .startSettings(
                            getActivity(), LocationPermissionSubpageSettings.class, fragmentArgs);
        } else if (mWebsiteSettingsObserver != null) {
            mWebsiteSettingsObserver.onLocationPermissionSubpageClicked();
        } else {
            assert false : "Not reached.";
        }
    }

    @RequiresNonNull({"mSite"})
    private void setUpClearDataPreference() {
        ClearWebsiteStorage preference = findPreference(PREF_CLEAR_DATA);
        long usage = mSite.getTotalUsage();
        int cookies = mSite.getNumberOfCookies();
        // Only take cookies into account when the new UI is enabled.
        if (usage > 0 || cookies > 0) {
            boolean appFound =
                    getSiteSettingsDelegate()
                            .getOriginsWithInstalledApp()
                            .contains(mSite.getAddress().getOrigin());
            Context context = preference.getContext();
            preference.setTitle(SiteSettingsUtil.generateStorageUsageText(context, usage, cookies));
            preference.setDataForDisplay(mSite.getTitle(), appFound, /* isGroup= */ false);
            if (mSite.isCookieDeletionDisabled(getBrowserContextHandle())) {
                preference.setEnabled(false);
            }
        } else {
            getPreferenceScreen().removePreference(preference);
        }
    }

    @RequiresNonNull({"mSite"})
    private void setupResetSitePreference() {
        Preference preference = findPreference(PREF_RESET_SITE);
        if (mHideNonPermissionPreferences) {
            preference.setWidgetLayoutResource(R.layout.reset_permissions_preference);
        }
        preference.setTitle(
                mHideNonPermissionPreferences
                        ? R.string.page_info_permissions_reset
                        : R.string.website_reset_full);
        preference.setOrder(mMaxPermissionOrder + 1);
        preference.setOnPreferenceClickListener(this);
        if (mSite.isCookieDeletionDisabled(getBrowserContextHandle())) {
            preference.setEnabled(false);
        }
    }

    private Intent getSettingsIntent(
            @Nullable String packageName, @ContentSettingsType.EnumType int type) {
        Intent intent = new Intent();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && type == ContentSettingsType.NOTIFICATIONS) {
            intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, packageName);
        } else {
            intent.setAction(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
            intent.setData(Uri.parse("package:" + packageName));
        }
        return intent;
    }

    /**
     * Replaces a Preference with a read-only copy. The new Preference retains its key and the order
     * within the preference screen, but gets a new summary and (intentionally) loses its click
     * handler.
     *
     * @return A read-only copy of the preference passed in as |oldPreference|.
     */
    @RequiresNonNull({"mSite"})
    private ChromeImageViewPreference createReadOnlyCopyOf(
            Preference oldPreference, String newSummary, @ContentSetting @Nullable Integer value) {
        ChromeImageViewPreference newPreference =
                new ChromeImageViewPreference(oldPreference.getContext());
        newPreference.setKey(oldPreference.getKey());
        setUpPreferenceCommon(newPreference, value);
        newPreference.setSummary(newSummary);
        @ContentSettingsType.EnumType
        int contentType = getContentSettingsTypeFromPreferenceKey(newPreference.getKey());
        if (contentType == mHighlightedPermission) {
            newPreference.setBackgroundColor(mHighlightColor);
        }

        return newPreference;
    }

    /**
     * A permission can be managed by an app. For example, with a Chrome SiteSettingsDelegate,
     * Notifications could be controlled by PWA.
     */
    @RequiresNonNull({"mSite"})
    private boolean setupAppDelegatePreference(
            Preference preference,
            @StringRes int contentDescriptionRes,
            @ContentSettingsType.EnumType int type,
            @ContentSetting @Nullable Integer value) {
        Origin origin = Origin.create(mSite.getAddress().getOrigin());
        if (origin == null) {
            return false;
        }

        String managedByAppName =
                getSiteSettingsDelegate().getDelegateAppNameForOrigin(origin, type);
        if (managedByAppName == null) {
            return false;
        }

        final Intent settingsIntent =
                getSettingsIntent(
                        getSiteSettingsDelegate().getDelegatePackageNameForOrigin(origin, type),
                        type);
        String summaryText = getString(R.string.website_setting_managed_by_app, managedByAppName);
        ChromeImageViewPreference newPreference =
                createReadOnlyCopyOf(preference, summaryText, value);

        newPreference.setImageView(R.drawable.permission_popups, contentDescriptionRes, null);
        // By disabling the ImageView, clicks will go through to the preference.
        newPreference.setImageViewEnabled(false);

        newPreference.setOnPreferenceClickListener(
                unused -> {
                    startActivity(settingsIntent);
                    return true;
                });
        return true;
    }

    @RequiresNonNull({"mSite"})
    private void setUpNotificationsPreference(Preference preference, boolean isEmbargoed) {
        @ContentSettingsType.EnumType int notificationType = ContentSettingsType.NOTIFICATIONS;
        final @ContentSetting @Nullable Integer value =
                mSite.getContentSetting(getBrowserContextHandle(), notificationType);
        // If `mHasRequestedNotificationsPermission`is true, this means the user clicked on the
        // "Manage" button in the notification permission prompt, and we should display the
        // permission request UI in PageInfo. `setupAppDelegatePreference` should not be called if
        // there is an active permission request.
        if (!mHasRequestedNotificationsPermission
                && setupAppDelegatePreference(
                        preference,
                        R.string.website_notification_settings,
                        notificationType,
                        value)) {
            return;
        }

        // TODO(crbug.com/458351800): Android O is deprecated, so this check can be removed.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // `mHasRequestedNotificationsPermission` indicates that the notification permission is
            // currently being requested, as it is not technically allowed yet, we should display
            // the "BLOCK" state. Because the requested permission's state is ASK, `mSite` will not
            // contain a value for this permission.
            if (mHasRequestedNotificationsPermission) {
                String overrideSummary =
                        getString(
                                ContentSettingsResources.getCategorySummary(
                                        ContentSetting.BLOCK, isOneTime(notificationType)));
                ChromeButtonPreference buttonPreference =
                        replaceWithReadOnlyButtonPreference(
                                preference, overrideSummary, ContentSetting.BLOCK);
                buttonPreference.setButton(
                        R.string.notifications_permission_subscribe,
                        R.string.notifications_permission_subscribe_a11y,
                        view -> {
                            if (mWebsiteSettingsObserver != null) {
                                mWebsiteSettingsObserver.onNotificationSubscribeClicked();
                            }

                            // Reset the requested permission state to false, as the permission has
                            // been granted and is not longer in request.
                            mHasRequestedNotificationsPermission = false;

                            if (mSite != null) {
                                displaySitePermissions();
                            }
                        });
                return;
            }

            if (value == null || (value != ContentSetting.ALLOW && value != ContentSetting.BLOCK)) {
                // TODO(crbug.com/40526685): Figure out if this is the correct thing to do, for
                // values that are non-null, but not ALLOW or BLOCK either. (In
                // setupContentSettingsPreference we treat non-ALLOW settings as BLOCK, but here we
                // are simply not adding it.)
                return;
            }

            String overrideSummary =
                    isEmbargoed
                            ? getString(R.string.automatically_blocked)
                            : getString(
                                    ContentSettingsResources.getCategorySummary(
                                            value, isOneTime(notificationType)));

            // On Android O this preference is read-only, so we replace the existing pref with a
            // regular Preference that takes users to OS settings on click.
            ChromeImageViewPreference newPreference =
                    createReadOnlyCopyOf(preference, overrideSummary, value);
            newPreference.setImageView(
                    R.drawable.permission_popups,
                    R.string.website_notification_settings,
                    unused -> launchOsChannelSettingsFromPreference(preference));
            newPreference.setImageColor(R.color.default_icon_color_secondary_tint_list);
            newPreference.setDefaultValue(value);

            newPreference.setOnPreferenceClickListener(
                    unused -> {
                        launchOsChannelSettingsFromPreference(preference);
                        return true;
                    });
        } else {
            setupContentSettingsPreference(
                    preference, value, isEmbargoed, isOneTime(notificationType));
        }
    }

    /**
     * Replaces a Preference with a read-only copy. The new Preference retains its key and the order
     * within the preference screen, but gets a new summary and (intentionally) loses its click
     * handler.
     *
     * @return A read-only copy of the preference passed in as |oldPreference|.
     */
    @RequiresNonNull({"mSite"})
    private ChromeButtonPreference replaceWithReadOnlyButtonPreference(
            Preference oldPreference, String newSummary, @ContentSetting @Nullable Integer value) {
        ChromeButtonPreference newPreference =
                new ChromeButtonPreference(oldPreference.getContext(), null);
        newPreference.setKey(oldPreference.getKey());
        setUpPreferenceCommon(newPreference, value);
        newPreference.setSummary(newSummary);
        @ContentSettingsType.EnumType
        int contentType = getContentSettingsTypeFromPreferenceKey(newPreference.getKey());
        if (contentType == mHighlightedPermission) {
            newPreference.setBackgroundColor(mHighlightColor);
        }

        return newPreference;
    }

    // This is implemented as a public utility function to better facilitate testing.
    @VisibleForTesting
    public void launchOsChannelSettingsFromPreference(Preference preference) {
        // There is no notification channel if the origin is merely embargoed. Create it
        // just-in-time if the user tries to change to setting.
        if (assumeNonNull(mSite).isEmbargoed(ContentSettingsType.NOTIFICATIONS)) {
            mSite.setContentSetting(
                    getBrowserContextHandle(),
                    ContentSettingsType.NOTIFICATIONS,
                    ContentSetting.BLOCK);
        }

        // There is no guarantee that a channel has been initialized yet for sites
        // that were granted permission before the channel-initialization-on-grant
        // code was in place. However, getChannelIdForOrigin will fall back to the
        // generic Sites channel if no specific channel has been created for the given
        // origin, so it is safe to open the channel settings for whatever channel ID
        // it returns.
        getSiteSettingsDelegate()
                .getChannelIdForOrigin(
                        mSite.getAddress().getOrigin(),
                        (channelId) -> {
                            assumeNonNull(mSite);
                            launchOsChannelSettings(preference.getContext(), channelId);
                        });
    }

    @RequiresNonNull({"mSite"})
    private void launchOsChannelSettings(Context context, String channelId) {
        // Store current value of permission to allow comparison against new value at return.
        mPreviousNotificationPermission =
                mSite.getContentSetting(
                        getBrowserContextHandle(), ContentSettingsType.NOTIFICATIONS);

        Intent intent = new Intent(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS);
        intent.putExtra(Settings.EXTRA_CHANNEL_ID, channelId);
        intent.putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName());
        startActivityForResult(intent, REQUEST_CODE_NOTIFICATION_CHANNEL_SETTINGS);
    }

    /**
     * If we are returning to Site Settings from another activity, the preferences displayed may be
     * out of date. Here we refresh any we suspect may have changed.
     */
    @Override
    public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        // The preference screen and mSite may be null if this activity was killed in the
        // background, and the tasks scheduled from onActivityCreated haven't completed yet. Those
        // tasks will take care of reinitializing everything afresh so there is no work to do here.
        if (getPreferenceScreen() == null || mSite == null) {
            return;
        }
        if (requestCode == REQUEST_CODE_NOTIFICATION_CHANNEL_SETTINGS) {
            @ContentSetting
            Integer newPermission =
                    mSite.getContentSetting(
                            getBrowserContextHandle(), ContentSettingsType.NOTIFICATIONS);
            assumeNonNull(newPermission);
            // User has navigated back from system channel settings on O+. Ensure notification
            // preference is up to date, since they might have toggled it from channel settings.
            Preference notificationsPreference =
                    findPreference(
                            assumeNonNull(getPreferenceKey(ContentSettingsType.NOTIFICATIONS)));
            if (notificationsPreference != null) {
                onPreferenceChange(notificationsPreference, newPermission);
            }

            // To ensure UMA receives notification revocations, we detect if the setting has changed
            // after returning to Chrome.  This is lossy, as it will miss when users revoke a
            // permission, but do not return immediately to Chrome (e.g. they close the permissions
            // activity, instead of hitting the back button), but prevents us from having to check
            // for changes each time Chrome becomes active.
            if (assumeNonNull(mPreviousNotificationPermission) == ContentSetting.ALLOW
                    && newPermission != ContentSetting.ALLOW) {
                org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                        .reportNotificationRevokedForOrigin(
                                getBrowserContextHandle(),
                                mSite.getAddress().getOrigin(),
                                newPermission);
                mPreviousNotificationPermission = null;
            }
        }
    }

    /**
     * Creates a ChromeImageViewPreference for each object permission with a
     * ManagedPreferenceDelegate that configures the Preference's widget to display a managed icon
     * and show a toast if a managed permission is clicked. The preferences are added to the
     * preference screen using |maxPermissionOrder| to order the preferences in the list.
     */
    @RequiresNonNull({"mSite"})
    private void setUpChosenObjectPreferences() {
        PreferenceScreen preferenceScreen = getPreferenceScreen();

        for (ChosenObjectInfo info : mSite.getChosenObjectInfo()) {
            ChromeImageViewPreference preference =
                    new ChromeImageViewPreference(getStyledContext());
            assert arrayContains(
                    SiteSettingsUtil.CHOOSER_PERMISSIONS, info.getContentSettingsType());
            mChooserPermissionPreferences.add(preference);
            preference.setIcon(getContentSettingsIcon(info.getContentSettingsType(), null));
            preference.setTitle(info.getName());
            preference.setImageView(
                    R.drawable.ic_delete_white_24dp,
                    R.string.website_settings_revoke_device_permission,
                    (View view) -> {
                        info.revoke(getBrowserContextHandle());
                        preferenceScreen.removePreference(preference);
                        mChooserPermissionPreferences.remove(preference);

                        if (!hasPermissionsPreferences()) {
                            removePreferenceSafely(PREF_PERMISSIONS_HEADER);
                        }
                    });
            if (info.getContentSettingsType() == mHighlightedPermission) {
                preference.setBackgroundColor(mHighlightColor);
            }

            preference.setManagedPreferenceDelegate(
                    new ForwardingManagedPreferenceDelegate(
                            getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return info.isManaged();
                        }
                    });

            preference.setOrder(++mMaxPermissionOrder);
            preferenceScreen.addPreference(preference);
        }
    }

    String getEmbeddedPermissionSummary(
            @Nullable String embeddedHost, @ContentSetting int setting) {
        int id =
                setting == ContentSetting.ALLOW
                        ? R.string.website_settings_site_allowed
                        : R.string.website_settings_site_blocked;
        return getContext().getString(id, embeddedHost);
    }

    @RequiresNonNull({"mSite"})
    private void setUpEmbeddedContentSettingPreferences() {
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        BrowserContextHandle handle = getBrowserContextHandle();

        for (List<ContentSettingException> entries : mSite.getEmbeddedPermissions().values()) {
            for (ContentSettingException info : entries) {
                @ContentSetting int contentSetting = info.getContentSetting();
                assert arrayContains(
                        SiteSettingsUtil.EMBEDDED_PERMISSIONS, info.getContentSettingType());
                var preference = new ChromeSwitchPreference(getStyledContext());
                mEmbeddedPermissionPreferences.add(preference);
                preference.setIcon(
                        getContentSettingsIcon(info.getContentSettingType(), contentSetting));
                preference.setTitle(
                        ContentSettingsResources.getTitle(info.getContentSettingType()));
                var pattern = assumeNonNull(WebsiteAddress.create(info.getPrimaryPattern()));
                preference.setSummary(
                        getEmbeddedPermissionSummary(pattern.getHost(), contentSetting));

                preference.setChecked(contentSetting == ContentSetting.ALLOW);
                preference.setOnPreferenceChangeListener(
                        (pref, newValue) -> {
                            @ContentSetting
                            int newContentSetting =
                                    (boolean) newValue
                                            ? ContentSetting.ALLOW
                                            : ContentSetting.BLOCK;
                            info.setContentSetting(handle, newContentSetting);
                            preference.setSummary(
                                    getEmbeddedPermissionSummary(
                                            pattern.getHost(), newContentSetting));
                            preference.setIcon(
                                    getContentSettingsIcon(
                                            info.getContentSettingType(), newContentSetting));

                            if (mWebsiteSettingsObserver != null) {
                                mWebsiteSettingsObserver.onPermissionChanged();
                            }
                            return true;
                        });
                if (info.getContentSettingType() == mHighlightedPermission) {
                    preference.setBackgroundColor(
                            AppCompatResources.getColorStateList(getContext(), mHighlightColor)
                                    .getDefaultColor());
                }

                preference.setOrder(++mMaxPermissionOrder);
                preferenceScreen.addPreference(preference);
            }
        }
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    @RequiresNonNull({"mSite"})
    private void setUpOsWarningPreferences(@Nullable SiteSettingsCategory categoryWithWarning) {
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        // Remove the 'permission is off in Android' message if not needed.
        if (categoryWithWarning == null) {
            removePreferenceSafely(PREF_OS_PERMISSIONS_WARNING);
            removePreferenceSafely(PREF_OS_PERMISSIONS_WARNING_EXTRA);
            removePreferenceSafely(PREF_OS_PERMISSIONS_WARNING_DIVIDER);
        } else {
            Preference osWarning = findPreference(PREF_OS_PERMISSIONS_WARNING);
            Preference osWarningExtra = findPreference(PREF_OS_PERMISSIONS_WARNING_EXTRA);
            categoryWithWarning.configureWarningPreferences(
                    osWarning,
                    osWarningExtra,
                    getContext(),
                    getSiteSettingsDelegate().getAppName());
            if (osWarning.getTitle() == null) {
                preferenceScreen.removePreference(osWarning);
            } else if (osWarningExtra.getTitle() == null) {
                preferenceScreen.removePreference(osWarningExtra);
            }
        }
    }

    private void setUpRelatedSitesPreferences() {
        PreferenceCategory relatedSitesSection = findPreference(PREF_RELATED_SITES_HEADER);
        TextMessagePreference relatedSitesText = new TextMessagePreference(getContext(), null);
        var rwsInfo = assumeNonNull(mSite).getRwsCookieInfo();
        boolean shouldRelatedSitesPrefBeVisible =
                getSiteSettingsDelegate().isRelatedWebsiteSetsDataAccessEnabled()
                        && rwsInfo != null;
        relatedSitesSection.setVisible(shouldRelatedSitesPrefBeVisible);
        relatedSitesText.setVisible(shouldRelatedSitesPrefBeVisible);

        if (shouldRelatedSitesPrefBeVisible) {
            assumeNonNull(rwsInfo);
            relatedSitesText.setManagedPreferenceDelegate(
                    new ForwardingManagedPreferenceDelegate(
                            getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return getSiteSettingsDelegate()
                                    .isPartOfManagedRelatedWebsiteSet(
                                            assumeNonNull(mSite).getAddress().getOrigin());
                        }
                    });

            relatedSitesText.setTitle(
                    getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.allsites_rws_summary,
                                    rwsInfo.getMembersCount(),
                                    Integer.toString(rwsInfo.getMembersCount()),
                                    rwsInfo.getOwner()));
            relatedSitesSection.addPreference(relatedSitesText);
        }
    }

    private void setupFileEditingGrants(boolean setOrder) {
        assumeNonNull(mSite);
        FileEditingInfo info = mSite.getFileEditingInfo();
        if (info == null || info.getGrants() == null || info.getGrants().isEmpty()) {
            removePreferenceSafely(PREF_FILE_EDITING_GRANTS);
            return;
        }

        PreferenceCategory header = findPreference(PREF_FILE_EDITING_GRANTS);
        if (setOrder) {
            header.setOrder(++mMaxPermissionOrder);
        }
        header.removeAll();
        for (FileEditingInfo.Grant grant : info.getGrants()) {
            ChromeImageViewPreference row = new ChromeImageViewPreference(getContext());
            row.setIcon(SettingsUtils.getTintedIcon(getContext(), R.drawable.ic_file_type_24));
            row.setTitle(grant.getDisplayName());
            row.setImageView(
                    R.drawable.ic_delete_white_24dp,
                    getContext()
                            .getString(
                                    R.string.website_settings_file_editing_grant_revoke,
                                    grant.getDisplayName()),
                    (View view) -> {
                        info.revokeGrant(getSiteSettingsDelegate(), grant);
                        setupFileEditingGrants(/* setOrder= */ false);
                    });
            header.addPreference(row);
        }
    }

    @RequiresNonNull({"mSite"})
    private void setUpAdsInformationalBanner() {
        // Add the informational banner which shows at the top of the UI if ad blocking is
        // activated on this site.
        boolean adBlockingActivated =
                SiteSettingsCategory.adsCategoryEnabled()
                        && WebsitePreferenceBridge.getAdBlockingActivated(
                                getBrowserContextHandle(), mSite.getAddress().getOrigin())
                        && findPreference(assumeNonNull(getPreferenceKey(ContentSettingsType.ADS)))
                                != null;

        if (!adBlockingActivated) {
            removePreferenceSafely(PREF_INTRUSIVE_ADS_INFO);
            removePreferenceSafely(PREF_INTRUSIVE_ADS_INFO_DIVIDER);
        }
    }

    @RequiresNonNull({"mSite"})
    private @Nullable SiteSettingsCategory getWarningCategory() {
        BrowserContextHandle browserContextHandle = getBrowserContextHandle();
        List<SiteSettingsCategory> warningCategories = new ArrayList<>();
        for (@SiteSettingsCategory.Type
        int type :
                new int[] {
                    SiteSettingsCategory.Type.DEVICE_LOCATION,
                    SiteSettingsCategory.Type.CAMERA,
                    SiteSettingsCategory.Type.MICROPHONE,
                    SiteSettingsCategory.Type.NOTIFICATIONS,
                    SiteSettingsCategory.Type.NFC,
                    SiteSettingsCategory.Type.HAND_TRACKING,
                    SiteSettingsCategory.Type.AUGMENTED_REALITY,
                    SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER
                }) {
            @Nullable SiteSettingsCategory category =
                    getWarningCategoryFor(SiteSettingsCategory.contentSettingsType(type));
            if (category != null && category.showPermissionBlockedMessage(getContext())) {
                warningCategories.add(category);
            }
        }

        if (warningCategories.isEmpty()) {
            return null;
        }

        if (warningCategories.size() > 1) {
            // Generic warning case: return one category.
            return new SiteSettingsCategory.GenericSiteSettingsCategory(browserContextHandle);
        }

        return warningCategories.get(0);
    }

    @RequiresNonNull({"mSite"})
    private @Nullable SiteSettingsCategory getWarningCategoryFor(
            @ContentSettingsType.EnumType int contentType) {
        BrowserContextHandle browserContextHandle = getBrowserContextHandle();
        if (contentType == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
            PermissionInfo info = mSite.getPermissionInfo(contentType);
            if (info == null) {
                return null;
            }

            GeolocationSetting permission = info.getGeolocationSetting(browserContextHandle);
            if (permission.mApproximate == ContentSetting.BLOCK) {
                return null;
            }
            return SiteSettingsCategory.createForDeviceLocation(
                    browserContextHandle, permission.mPrecise == ContentSetting.ALLOW);
        }
        @ContentSetting
        Integer permission = mSite.getContentSetting(browserContextHandle, contentType);

        if (permission == null || permission == ContentSetting.BLOCK) {
            return null;
        }
        return SiteSettingsCategory.createFromContentSettingsType(
                browserContextHandle, contentType);
    }

    private boolean hasUsagePreferences() {
        // New actions under the Usage preference category must be listed here so that the category
        // heading can be removed when no actions are shown.
        return findPreference(PREF_CLEAR_DATA) != null;
    }

    private boolean hasPermissionsPreferences() {
        if (!mChooserPermissionPreferences.isEmpty() || !mEmbeddedPermissionPreferences.isEmpty()) {
            return true;
        }
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        for (int i = 0; i < preferenceScreen.getPreferenceCount(); i++) {
            String key = preferenceScreen.getPreference(i).getKey();
            if (getContentSettingsTypeFromPreferenceKey(key) != ContentSettingsType.DEFAULT) {
                return true;
            }
        }
        return false;
    }

    @RequiresNonNull({"mSite"})
    private void setupContentSettingsPreference(
            Preference preference,
            @ContentSetting @Nullable Integer value,
            boolean isEmbargoed,
            boolean isOneTime) {
        @ContentSettingsType.EnumType
        int contentType = getContentSettingsTypeFromPreferenceKey(preference.getKey());
        if (contentType == ContentSettingsType.NOTIFICATIONS
                && mHasRequestedNotificationsPermission) {
            // `mHasRequestedNotificationsPermission` indicates that the notification permission is
            // currently being requested, as it is not technically allowed yet, we should display
            // the "BLOCK" state. Because the requested permission's state is ASK, `mSite` will not
            // contain a value for this permission.
            value = ContentSetting.BLOCK;
        }

        if (value == null) return;
        setUpPreferenceCommon(preference, value);
        preference.setOnPreferenceChangeListener(this);

        String summary;
        if (isEmbargoed) {
            summary = getString(R.string.automatically_blocked);
        } else if (contentType == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {

            LocationCategory locationCategory =
                    new LocationCategory(getBrowserContextHandle(), !mHasApproximateLocationGrant);
            summary =
                    getString(
                            ContentSettingsResources.getCategorySummary(
                                    contentType,
                                    value,
                                    isOneTime,
                                    mHasApproximateLocationGrant,
                                    locationCategory.hasPreciseOnlyBlockedWarning(getContext())));
        } else {
            summary =
                    getString(
                            ContentSettingsResources.getCategorySummary(
                                    contentType, value, isOneTime, mHasApproximateLocationGrant));
        }
        preference.setSummary(summary);
        if (preference instanceof ChromeImageViewPreference) {
            ChromeImageViewPreference oneTimePreference = (ChromeImageViewPreference) preference;
            oneTimePreference.setImageView(
                    R.drawable.material_ic_close_24dp,
                    R.string.website_settings_revoke_permission,
                    (View view) -> {
                        assumeNonNull(mSite);
                        PermissionInfo permissionInfo =
                                assumeNonNull(mSite.getPermissionInfo(contentType));
                        if (contentType == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
                            permissionInfo.setGeolocationSetting(getBrowserContextHandle(), null);
                        } else {
                            permissionInfo.setContentSetting(
                                    getBrowserContextHandle(), ContentSetting.DEFAULT);
                        }
                        getPreferenceScreen().removePreference(oneTimePreference);
                        if (mWebsiteSettingsObserver != null) {
                            mWebsiteSettingsObserver.onPermissionChanged();
                        }
                    });
            if (contentType == mHighlightedPermission) {
                oneTimePreference.setBackgroundColor(mHighlightColor);
            }
        } else {
            ChromeSwitchPreference switchPreference = (ChromeSwitchPreference) preference;
            switchPreference.setChecked(value == getEnabledValue(contentType));
            if (contentType == mHighlightedPermission) {
                switchPreference.setBackgroundColor(
                        AppCompatResources.getColorStateList(getContext(), mHighlightColor)
                                .getDefaultColor());
            }
        }
    }

    /**
     * Sets some properties that apply to both regular Preferences and ChromeSwitchPreferences, i.e.
     * preference title, enabled-state, and icon, based on the preference's key.
     */
    @RequiresNonNull({"mSite"})
    private void setUpPreferenceCommon(
            Preference preference, @ContentSetting @Nullable Integer value) {
        @ContentSettingsType.EnumType
        int contentType = getContentSettingsTypeFromPreferenceKey(preference.getKey());
        int titleResourceId = ContentSettingsResources.getTitle(contentType);

        if (contentType == ContentSettingsType.JAVASCRIPT_OPTIMIZER) {
            titleResourceId = R.string.website_settings_single_website_javascript_optimizer_toggle;
        }

        if (titleResourceId != 0) {
            preference.setTitle(titleResourceId);
        }

        SiteSettingsCategory category = getWarningCategoryFor(contentType);
        boolean showWarning =
                category != null && category.showPermissionBlockedMessage(getActivity());

        if (showWarning) {
            assumeNonNull(category);
            preference.setIcon(category.getDisabledInAndroidIcon(getContext()));
            if (contentType != ContentSettingsType.GEOLOCATION_WITH_OPTIONS
                    || ((category instanceof LocationCategory)
                            && !((LocationCategory) category)
                                    .hasPreciseOnlyBlockedWarning(getContext()))) {
                // The location toggle is not disabled in this case since the user can still toggle
                // between precise and approximate.
                preference.setEnabled(false);
            }
        } else {
            preference.setIcon(getContentSettingsIcon(contentType, value));
        }

        // These preferences are persisted elsewhere, using SharedPreferences
        // can cause issues with keys matching up with value.
        preference.setPersistent(false);
        preference.setOrder(++mMaxPermissionOrder);
        getPreferenceScreen().addPreference(preference);
    }

    @RequiresNonNull({"mSite"})
    private void setUpLocationPreference(Preference preference) {
        if (PermissionsAndroidFeatureMap.isEnabled(
                PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)) {
            return;
        }
        @ContentSetting
        @Nullable Integer permission =
                mSite.getContentSetting(getBrowserContextHandle(), ContentSettingsType.GEOLOCATION);
        if (setupAppDelegatePreference(
                preference,
                R.string.website_location_settings,
                ContentSettingsType.GEOLOCATION,
                permission)) {
            return;
        }

        setupContentSettingsPreference(
                preference,
                permission,
                mSite.isEmbargoed(ContentSettingsType.GEOLOCATION),
                isOneTime(ContentSettingsType.GEOLOCATION));
    }

    @RequiresNonNull({"mSite"})
    private void setUpLocationWithOptionsPreference(Preference preference) {
        if (!PermissionsAndroidFeatureMap.isEnabled(
                PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)) {
            return;
        }
        var info = mSite.getPermissionInfo(ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
        if (info == null) {
            return;
        }
        GeolocationSetting permission = info.getGeolocationSetting(getBrowserContextHandle());

        mHasApproximateLocationGrant =
                permission.mApproximate == ContentSetting.ALLOW
                        && permission.mApproximate != permission.mPrecise;

        if (preference instanceof TwoActionSwitchPreference) {
            ((TwoActionSwitchPreference) preference)
                    .setPrimaryButtonClickListener(
                            permission.mApproximate == ContentSetting.BLOCK
                                    ? null
                                    : (v) -> openLocationPermissionSubpage());
        }

        if (setupAppDelegatePreference(
                preference,
                R.string.website_location_settings,
                ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                permission.mApproximate)) {
            return;
        }

        setupContentSettingsPreference(
                preference,
                permission.mApproximate,
                mSite.isEmbargoed(ContentSettingsType.GEOLOCATION_WITH_OPTIONS),
                isOneTime(ContentSettingsType.GEOLOCATION_WITH_OPTIONS));
    }

    @RequiresNonNull({"mSite"})
    private void setUpSoundPreference(Preference preference) {
        if (!getArguments().getBoolean(EXTRA_SHOW_SOUND, true)) {
            return;
        }

        BrowserContextHandle browserContextHandle = getBrowserContextHandle();
        @ContentSetting
        @Nullable Integer currentValue =
                mSite.getContentSetting(browserContextHandle, ContentSettingsType.SOUND);
        // In order to always show the sound permission, set it up with the default value if it
        // doesn't have a current value.
        if (currentValue == null) {
            currentValue =
                    WebsitePreferenceBridge.isCategoryEnabled(
                                    browserContextHandle, ContentSettingsType.SOUND)
                            ? ContentSetting.ALLOW
                            : ContentSetting.BLOCK;
        }
        // Not possible to embargo SOUND.
        setupContentSettingsPreference(
                preference,
                currentValue,
                /* isEmbargoed= */ false,
                isOneTime(ContentSettingsType.SOUND));
    }

    @RequiresNonNull({"mSite"})
    private void setUpAutoPictureInPicturePreference(Preference preference) {
        if (!PermissionsAndroidFeatureMap.isEnabled(
                PermissionsAndroidFeatureList.AUTO_PICTURE_IN_PICTURE_ANDROID)) {
            return;
        }
        if (!getArguments().getBoolean(EXTRA_SHOW_AUTO_PIP, true)) {
            return;
        }

        BrowserContextHandle browserContextHandle = getBrowserContextHandle();
        @ContentSetting
        @Nullable Integer currentValue =
                mSite.getContentSetting(
                        browserContextHandle, ContentSettingsType.AUTO_PICTURE_IN_PICTURE);
        // In order to always show the auto-pip permission, set it up with the default value if it
        // doesn't have a current value. When the profile is incognito or the global content
        // setting is disabled, auto-pip is blocked by default.
        if (currentValue == null) {
            currentValue =
                    getSiteSettingsDelegate().isIncognito()
                                    || !WebsitePreferenceBridge.isCategoryEnabled(
                                            browserContextHandle,
                                            ContentSettingsType.AUTO_PICTURE_IN_PICTURE)
                            ? ContentSetting.BLOCK
                            : ContentSetting.ALLOW;
        }

        setupContentSettingsPreference(
                preference,
                currentValue,
                mSite.isEmbargoed(ContentSettingsType.AUTO_PICTURE_IN_PICTURE),
                isOneTime(ContentSettingsType.AUTO_PICTURE_IN_PICTURE));
    }

    @RequiresNonNull({"mSite"})
    private void setUpJavascriptPreference(Preference preference) {
        BrowserContextHandle browserContextHandle = getBrowserContextHandle();
        @ContentSetting
        @Nullable Integer currentValue =
                mSite.getContentSetting(browserContextHandle, ContentSettingsType.JAVASCRIPT);
        // If Javascript is blocked by default, then always show a Javascript permission.
        // To do this, set it to the default value (blocked).
        if ((currentValue == null)
                && !WebsitePreferenceBridge.isCategoryEnabled(
                        browserContextHandle, ContentSettingsType.JAVASCRIPT)) {
            currentValue = ContentSetting.BLOCK;
        }
        // Not possible to embargo JAVASCRIPT.
        setupContentSettingsPreference(
                preference,
                currentValue,
                /* isEmbargoed= */ false,
                isOneTime(ContentSettingsType.JAVASCRIPT));
    }

    /**
     * Updates the ads list preference based on whether the site is a candidate for blocking. This
     * has some custom behavior. 1. If the site is a candidate and has activation, the permission
     * should show up even if it is set as the default (e.g. |preference| is null). 2. The BLOCK
     * string is custom.
     */
    @RequiresNonNull({"mSite"})
    private void setUpAdsPreference(Preference preference) {
        BrowserContextHandle browserContextHandle = getBrowserContextHandle();
        // Do not show the setting if the category is not enabled.
        if (!SiteSettingsCategory.adsCategoryEnabled()) {
            setupContentSettingsPreference(
                    preference, null, false, isOneTime(ContentSettingsType.ADS));
            return;
        }
        // If the ad blocker is activated, then this site will have ads blocked unless there is an
        // explicit permission disallowing the blocking.
        boolean activated =
                WebsitePreferenceBridge.getAdBlockingActivated(
                        browserContextHandle, mSite.getAddress().getOrigin());
        @ContentSetting
        @Nullable Integer permission =
                mSite.getContentSetting(browserContextHandle, ContentSettingsType.ADS);

        // If |permission| is null, there is no explicit (non-default) permission set for this site.
        // If the site is not considered a candidate for blocking, do the standard thing and remove
        // the preference.
        if (permission == null && !activated) {
            setupContentSettingsPreference(
                    preference, null, false, isOneTime(ContentSettingsType.ADS));
            return;
        }

        // However, if the blocking is activated, we still want to show the permission, even if it
        // is in the default state.
        if (permission == null) {
            permission =
                    WebsitePreferenceBridge.isCategoryEnabled(
                                    browserContextHandle, ContentSettingsType.ADS)
                            ? ContentSetting.ALLOW
                            : ContentSetting.BLOCK;
        }
        // Not possible to embargo ADS.
        setupContentSettingsPreference(
                preference,
                permission,
                /* isEmbargoed= */ false,
                isOneTime(ContentSettingsType.ADS));
    }

    public @ContentSettingsType.EnumType int getContentSettingsTypeFromPreferenceKey(
            String preferenceKey) {
        if (mPreferenceMap == null) {
            mPreferenceMap = new HashMap<>();
            for (@ContentSettingsType.EnumType int type = 0;
                    type <= ContentSettingsType.MAX_VALUE;
                    type++) {
                String key = getPreferenceKey(type);
                if (key != null) {
                    mPreferenceMap.put(key, type);
                }
            }
        }
        Integer type = mPreferenceMap.get(preferenceKey);
        if (type != null) return type;
        return ContentSettingsType.DEFAULT;
    }

    private void popBackIfNoSettings() {
        if (!hasPermissionsPreferences() && !hasUsagePreferences() && getActivity() != null) {
            popBackToPreviousPage();
        }
    }

    private void popBackToPreviousPage() {
        // Save the paused fragment before finishing the current fragment as it may cause the
        // paused fragment to resume.
        GroupedWebsitesSettings groupFragment = GroupedWebsitesSettings.getPausedInstance();
        Activity activity = getActivity();
        if (activity != null) {
            var settingsNavigation = assumeNonNull(getSettingsNavigation());
            settingsNavigation.finishCurrentSettings(this);
            if (mFromGrouped && groupFragment != null) {
                settingsNavigation.executePendingNavigations(activity);
                settingsNavigation.finishCurrentSettings(groupFragment);
            }
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        // It is possible that this UI is destroyed while a dialog is open because
        // incognito mode is closed through the system notification.
        if (getView() == null) return true;
        assumeNonNull(mSite);

        BrowserContextHandle browserContextHandle = getBrowserContextHandle();
        int type = getContentSettingsTypeFromPreferenceKey(preference.getKey());
        if (type == ContentSettingsType.DEFAULT) return false;

        @ContentSetting int permission;
        if (newValue instanceof Boolean) {
            permission = (Boolean) newValue ? getEnabledValue(type) : ContentSetting.BLOCK;
        } else {
            permission = (Integer) newValue;
        }

        if (type == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
            PermissionInfo permissionInfo = assumeNonNull(mSite.getPermissionInfo(type));
            var oldSetting = permissionInfo.getGeolocationSetting(browserContextHandle);
            var newPreciseValue = permission;
            if (mHasApproximateLocationGrant && permission == ContentSetting.ALLOW) {
                newPreciseValue = oldSetting.mPrecise;
            }
            permissionInfo.setGeolocationSetting(
                    browserContextHandle,
                    new GeolocationSetting(
                            /* approximate= */ permission, /* precise= */ newPreciseValue));

            if (preference instanceof TwoActionSwitchPreference) {
                ((TwoActionSwitchPreference) preference)
                        .setPrimaryButtonClickListener(
                                permission == ContentSetting.BLOCK
                                        ? null
                                        : (v) -> openLocationPermissionSubpage());
            }
        } else {
            mSite.setContentSetting(browserContextHandle, type, permission);
        }

        boolean hasPreciseOnlyBlockedWarning = false;
        if (type == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
            LocationCategory locationCategory =
                    new LocationCategory(getBrowserContextHandle(), !mHasApproximateLocationGrant);
            hasPreciseOnlyBlockedWarning =
                    locationCategory.hasPreciseOnlyBlockedWarning(getContext());
        }

        // In Clank, one time grants are only possible via prompt, not via page
        // info.
        preference.setSummary(
                getString(
                        ContentSettingsResources.getCategorySummary(
                                type,
                                permission,
                                false,
                                mHasApproximateLocationGrant,
                                hasPreciseOnlyBlockedWarning)));
        preference.setIcon(getContentSettingsIcon(type, permission));

        if (mWebsiteSettingsObserver != null) {
            mWebsiteSettingsObserver.onPermissionChanged();
        }

        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        assumeNonNull(mSite);
        if (mHideNonPermissionPreferences) {
            showResetPermissionsOnlyDialog();
        } else {
            showClearAndResetDialog();
        }
        return true;
    }

    /** Resets the current site, clearing all permissions and storage used (inc. cookies). */
    @VisibleForTesting
    public void resetSite() {
        if (getActivity() == null) return;
        // Clear the screen.
        // TODO(mvanouwerkerk): Refactor this class so that it does not depend on the screen state
        // for its logic. This class should maintain its own data model, and only update the screen
        // after a change is made.
        for (@ContentSettingsType.EnumType int type = 0;
                type <= ContentSettingsType.MAX_VALUE;
                type++) {
            String key = getPreferenceKey(type);
            if (key != null) {
                removePreferenceSafely(key);
            }
        }
        for (var preference : mEmbeddedPermissionPreferences) {
            getPreferenceScreen().removePreference(preference);
        }
        mEmbeddedPermissionPreferences.clear();

        // Clearing stored data implies popping back to parent menu if there is nothing left to
        // show. Therefore, we only need to explicitly close the activity if there's no stored data
        // to begin with. The only exception to this is if there are policy managed permissions as
        // those cannot be reset and will always show.
        boolean finishActivityImmediately =
                assumeNonNull(mSite).getTotalUsage() == 0 && !hasManagedChooserPermissions();

        SiteDataCleaner.resetPermissions(getBrowserContextHandle(), mSite);
        SiteDataCleaner.clearData(getSiteSettingsDelegate(), mSite, mDataClearedCallback);

        RecordHistogram.recordEnumeratedHistogram(
                "Privacy.DeleteBrowsingData.Action",
                DeleteBrowsingDataAction.SITES_SETTINGS_PAGE,
                DeleteBrowsingDataAction.MAX_VALUE);
        if (finishActivityImmediately) {
            // Save the paused fragment before finishing the current fragment as it may cause the
            // paused fragment to resume.
            GroupedWebsitesSettings groupFragment = GroupedWebsitesSettings.getPausedInstance();
            Activity activity = getActivity();
            if (activity != null) {
                var settingsNavigation = assumeNonNull(getSettingsNavigation());
                settingsNavigation.finishCurrentSettings(this);
                if (mFromGrouped && groupFragment != null) {
                    settingsNavigation.executePendingNavigations(activity);
                    settingsNavigation.finishCurrentSettings(groupFragment);
                }
            }
        }
    }

    public boolean isOneTime(@ContentSettingsType.EnumType int type) {
        PermissionInfo permissionInfo = assumeNonNull(mSite).getPermissionInfo(type);
        return permissionInfo != null && permissionInfo.getSessionModel() == SessionModel.ONE_TIME;
    }

    /**
     * Ensures preference exists before removing to avoid NPE in
     * {@link PreferenceScreen#removePreference}.
     */
    private void removePreferenceSafely(CharSequence prefKey) {
        Preference preference = findPreference(prefKey);
        if (preference != null) getPreferenceScreen().removePreference(preference);
    }

    /** Removes any user granted chosen object preference(s) from the preference screen. */
    private void removeUserChosenObjectPreferences() {
        var it = mChooserPermissionPreferences.iterator();
        while (it.hasNext()) {
            var preference = it.next();
            if (preference != null && !((ChromeImageViewPreference) preference).isManaged()) {
                getPreferenceScreen().removePreference(preference);
                it.remove();
            }
        }

        if (hasManagedChooserPermissions()) {
            ManagedPreferencesUtils.showManagedSettingsCannotBeResetToast(getContext());
        }
    }

    @RequiresNonNull({"mSite"})
    private void showResetPermissionsOnlyDialog() {
        // Handle the reset preference click by showing a confirmation.
        mConfirmationDialog =
                new AlertDialog.Builder(getContext(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setTitle(R.string.page_info_permissions_reset_dialog_title)
                        .setMessage(
                                getString(
                                        R.string.page_info_permissions_reset_confirmation,
                                        mSite.getAddress().getHost()))
                        .setPositiveButton(
                                R.string.reset,
                                (dialog, which) -> {
                                    SiteDataCleaner.resetPermissions(
                                            getBrowserContextHandle(), assumeNonNull(mSite));
                                    if (mWebsiteSettingsObserver != null) {
                                        mWebsiteSettingsObserver.onPermissionsReset();
                                    }
                                })
                        .setNegativeButton(
                                R.string.cancel, (dialog, which) -> mConfirmationDialog = null)
                        .show();
    }

    @RequiresNonNull({"mSite"})
    private void showClearAndResetDialog() {
        // Handle a click on the Clear & Reset button.
        View dialogView =
                getActivity().getLayoutInflater().inflate(R.layout.clear_reset_dialog, null);
        TextView mainMessage = dialogView.findViewById(R.id.main_message);
        mainMessage.setText(
                getString(
                        R.string.website_single_reset_confirmation, mSite.getAddress().getHost()));
        TextView signedOutText = dialogView.findViewById(R.id.signed_out_text);
        signedOutText.setText(R.string.webstorage_clear_data_dialog_sign_out_message);
        TextView offlineText = dialogView.findViewById(R.id.offline_text);
        offlineText.setText(R.string.webstorage_delete_data_dialog_offline_message);
        mConfirmationDialog =
                new AlertDialog.Builder(getContext(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setView(dialogView)
                        .setTitle(R.string.website_reset_confirmation_title)
                        .setPositiveButton(
                                R.string.website_reset,
                                (dialog, which) -> {
                                    resetSite();
                                    if (mWebsiteSettingsObserver != null) {
                                        mWebsiteSettingsObserver.onPermissionsReset();
                                    }
                                })
                        .setNegativeButton(
                                R.string.cancel, (dialog, which) -> mConfirmationDialog = null)
                        .show();
    }

    boolean hasManagedChooserPermissions() {
        for (var preference : mChooserPermissionPreferences) {
            if (preference.isManaged()) {
                return true;
            }
        }
        return false;
    }

    private BrowserContextHandle getBrowserContextHandle() {
        return getSiteSettingsDelegate().getBrowserContextHandle();
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
