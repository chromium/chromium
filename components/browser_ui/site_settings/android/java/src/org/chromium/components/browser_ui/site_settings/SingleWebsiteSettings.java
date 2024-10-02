// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

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
import androidx.annotation.Nullable;
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
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Shows the permissions and other settings for a particular website. */
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
            default:
                return null;
        }
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
    private Observer mWebsiteSettingsObserver;

    // The website this page is displaying details about.
    private Website mSite;

    // Whether these settings were opened from GroupedWebsitesSettings.
    private boolean mFromGrouped;

    private final List<ChromeSwitchPreference> mEmbeddedPermissionPreferences = new ArrayList<>();

    private final List<ChromeImageViewPreference> mChooserPermissionPreferences = new ArrayList<>();

    // Records previous notification permission on Android O+ to allow detection of permission
    // revocation within the Android system permission activity.
    private @ContentSettingValues @Nullable Integer mPreviousNotificationPermission;

    // Map from preference key to ContentSettingsType.
    private Map<String, Integer> mPreferenceMap;

    private Dialog mConfirmationDialog;

    // Maximum value used for the order of the permissions
    private int mMaxPermissionOrder;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    private class SingleWebsitePermissionsPopulator
            implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        private final WebsiteAddress mSiteAddress;

        public SingleWebsitePermissionsPopulator(WebsiteAddress siteAddress) {
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
     * Creates a Bundle with the correct arguments for opening this fragment for
     * the website with the given url.
     *
     * @param url The URL to open the fragment with. This is a complete url including scheme,
     *            domain, port,  path, etc.
     * @return The bundle to attach to the preferences intent.
     */
    public static Bundle createFragmentArgsForSite(String url) {
        Bundle fragmentArgs = new Bundle();
        String origin = Origin.createOrThrow(url).toString();
        fragmentArgs.putSerializable(EXTRA_SITE_ADDRESS, WebsiteAddress.create(origin));
        return fragmentArgs;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
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
    public void onViewStateRestored(Bundle savedInstanceState) {
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
            if (getFragmentManager().isStateSaved()) {
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
                                    getSiteSettingsDelegate(), mSite, mDataClearedCallback);
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

    public void setHideNonPermissionPreferences(boolean hide) {
        mHideNonPermissionPreferences = hide;
    }

    public void setWebsiteSettingsObserver(Observer observer) {
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
        String host = Uri.parse(origin).getHost();
        String domainAndRegistry = address.getDomainAndRegistry();
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
                                                    origin, exception.getSecondaryPattern());
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
            if (merged.getRWSCookieInfo() == null
                    && other.getRWSCookieInfo() != null
                    && domainAndRegistry.equals(other.getAddress().getDomainAndRegistry())) {
                merged.setRWSCookieInfo(other.getRWSCookieInfo());
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

    private Drawable getContentSettingsIcon(
            @ContentSettingsType.EnumType int contentSettingsType,
            @ContentSettingValues @Nullable Integer value) {
        return ContentSettingsResources.getContentSettingsIcon(
                getContext(), contentSettingsType, value);
    }

    /**
     * Updates the permissions displayed in the UI by fetching them from mSite. Must only be called
     * once mSite is set.
     */
    private void displaySitePermissions() {
        if (getPreferenceScreen() != null) {
            getPreferenceScreen().removeAll();
        }
        SettingsUtils.addPreferencesFromResource(this, R.xml.single_website_preferences);

        findPreference(PREF_SITE_TITLE).setTitle(mSite.getTitle());
        setupContentSettingsPreferences();
        setUpEmbeddedContentSettingPreferences();
        setUpChosenObjectPreferences();
        setupResetSitePreference();
        setUpClearDataPreference();
        setUpOsWarningPreferences();
        setupRelatedSitesPreferences();

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

    private void setupContentSettingsPreferences() {
        mMaxPermissionOrder = findPreference(PREF_PERMISSIONS_HEADER).getOrder();
        for (@ContentSettingsType.EnumType int type : SiteSettingsUtil.SETTINGS_ORDER) {
            Preference preference = new ChromeSwitchPreference(getStyledContext());
            preference.setKey(getPreferenceKey(type));

            if (type == ContentSettingsType.ADS) {
                setUpAdsPreference(preference);
            } else if (type == ContentSettingsType.SOUND) {
                setUpSoundPreference(preference);
            } else if (type == ContentSettingsType.JAVASCRIPT) {
                setUpJavascriptPreference(preference);
            } else if (type == ContentSettingsType.GEOLOCATION) {
                setUpLocationPreference(preference);
            } else if (type == ContentSettingsType.NOTIFICATIONS) {
                setUpNotificationsPreference(preference, mSite.isEmbargoed(type));
            } else {
                setupContentSettingsPreference(
                        preference,
                        mSite.getContentSetting(
                                getSiteSettingsDelegate().getBrowserContextHandle(), type),
                        mSite.isEmbargoed(type),
                        isOneTime(type));
            }
        }
    }

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
            if (mSite.isCookieDeletionDisabled(
                    getSiteSettingsDelegate().getBrowserContextHandle())) {
                preference.setEnabled(false);
            }
        } else {
            getPreferenceScreen().removePreference(preference);
        }
    }

    private void setupResetSitePreference() {
        Preference preference = findPreference(PREF_RESET_SITE);
        preference.setTitle(
                mHideNonPermissionPreferences
                        ? R.string.page_info_permissions_reset
                        : R.string.website_reset_full);
        preference.setOrder(mMaxPermissionOrder + 1);
        preference.setOnPreferenceClickListener(this);
        if (mSite.isCookieDeletionDisabled(getSiteSettingsDelegate().getBrowserContextHandle())) {
            preference.setEnabled(false);
        }
    }

    private Intent getSettingsIntent(String packageName, @ContentSettingsType.EnumType int type) {
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
     * Replaces a Preference with a read-only copy. The new Preference retains
     * its key and the order within the preference screen, but gets a new
     * summary and (intentionally) loses its click handler.
     * @return A read-only copy of the preference passed in as |oldPreference|.
     */
    private ChromeImageViewPreference createReadOnlyCopyOf(
            Preference oldPreference,
            String newSummary,
            @ContentSettingValues @Nullable Integer value) {
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
     * Notifications could be controlled by PWA, however for a Weblayer variant, Location could be
     * controlled by the DSE.
     */
    private boolean setupAppDelegatePreference(
            Preference preference,
            @StringRes int contentDescriptionRes,
            @ContentSettingsType.EnumType int type,
            @ContentSettingValues @Nullable Integer value) {
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

    private void setUpNotificationsPreference(Preference preference, boolean isEmbargoed) {
        @ContentSettingsType.EnumType int notificationType = ContentSettingsType.NOTIFICATIONS;
        final @ContentSettingValues @Nullable Integer value =
                mSite.getContentSetting(
                        getSiteSettingsDelegate().getBrowserContextHandle(), notificationType);
        if (setupAppDelegatePreference(
                preference, R.string.website_notification_settings, notificationType, value)) {
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (value == null
                    || (value != ContentSettingValues.ALLOW
                            && value != ContentSettingValues.BLOCK)) {
                // TODO(crbug.com/40526685): Figure out if this is the correct thing to do, for
                // values
                // that are non-null, but not ALLOW or BLOCK either. (In
                // setupContentSettingsPreference we treat non-ALLOW settings as BLOCK, but here we
                // are simply not adding it.)
                return;
            }
            String overrideSummary;
            overrideSummary =
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
                    0,
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

    // This is implemented as a public utility function to better facilitate testing.
    @VisibleForTesting
    public void launchOsChannelSettingsFromPreference(Preference preference) {
        // There is no notification channel if the origin is merely embargoed. Create it
        // just-in-time if the user tries to change to setting.
        if (mSite.isEmbargoed(ContentSettingsType.NOTIFICATIONS)) {
            mSite.setContentSetting(
                    getSiteSettingsDelegate().getBrowserContextHandle(),
                    ContentSettingsType.NOTIFICATIONS,
                    ContentSettingValues.BLOCK);
        }

        // There is no guarantee that a channel has been initialized yet for sites
        // that were granted permission before the channel-initialization-on-grant
        // code was in place. However, getChannelIdForOrigin will fall back to the
        // generic Sites channel if no specific channel has been created for the given
        // origin, so it is safe to open the channel settings for whatever channel ID
        // it returns.
        String channelId =
                getSiteSettingsDelegate().getChannelIdForOrigin(mSite.getAddress().getOrigin());
        launchOsChannelSettings(preference.getContext(), channelId);
    }

    private void launchOsChannelSettings(Context context, String channelId) {
        // Store current value of permission to allow comparison against new value at return.
        mPreviousNotificationPermission =
                mSite.getContentSetting(
                        getSiteSettingsDelegate().getBrowserContextHandle(),
                        ContentSettingsType.NOTIFICATIONS);

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
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        // The preference screen and mSite may be null if this activity was killed in the
        // background, and the tasks scheduled from onActivityCreated haven't completed yet. Those
        // tasks will take care of reinitializing everything afresh so there is no work to do here.
        if (getPreferenceScreen() == null || mSite == null) {
            return;
        }
        if (requestCode == REQUEST_CODE_NOTIFICATION_CHANNEL_SETTINGS) {
            @ContentSettingValues
            int newPermission =
                    mSite.getContentSetting(
                            getSiteSettingsDelegate().getBrowserContextHandle(),
                            ContentSettingsType.NOTIFICATIONS);
            // User has navigated back from system channel settings on O+. Ensure notification
            // preference is up to date, since they might have toggled it from channel settings.
            Preference notificationsPreference =
                    findPreference(getPreferenceKey(ContentSettingsType.NOTIFICATIONS));
            if (notificationsPreference != null) {
                onPreferenceChange(notificationsPreference, (Object) newPermission);
            }

            // To ensure UMA receives notification revocations, we detect if the setting has changed
            // after returning to Chrome.  This is lossy, as it will miss when users revoke a
            // permission, but do not return immediately to Chrome (e.g. they close the permissions
            // activity, instead of hitting the back button), but prevents us from having to check
            // for changes each time Chrome becomes active.
            if (mPreviousNotificationPermission == ContentSettingValues.ALLOW
                    && newPermission != ContentSettingValues.ALLOW) {
                org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni.get()
                        .reportNotificationRevokedForOrigin(
                                getSiteSettingsDelegate().getBrowserContextHandle(),
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
                        info.revoke(getSiteSettingsDelegate().getBrowserContextHandle());
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

    String getEmbeddedPermissionSummary(String embeddedHost, @ContentSettingValues int setting) {
        int id =
                setting == ContentSettingValues.ALLOW
                        ? R.string.website_settings_site_allowed
                        : R.string.website_settings_site_blocked;
        return getContext().getResources().getString(id, embeddedHost);
    }

    private void setUpEmbeddedContentSettingPreferences() {
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        BrowserContextHandle handle = getSiteSettingsDelegate().getBrowserContextHandle();

        for (List<ContentSettingException> entries : mSite.getEmbeddedPermissions().values()) {
            for (ContentSettingException info : entries) {
                assert arrayContains(
                        SiteSettingsUtil.EMBEDDED_PERMISSIONS, info.getContentSettingType());
                var preference = new ChromeSwitchPreference(getStyledContext());
                mEmbeddedPermissionPreferences.add(preference);
                preference.setIcon(
                        getContentSettingsIcon(
                                info.getContentSettingType(), info.getContentSetting()));
                preference.setTitle(
                        ContentSettingsResources.getTitle(info.getContentSettingType()));
                var pattern = WebsiteAddress.create(info.getPrimaryPattern());
                preference.setSummary(
                        getEmbeddedPermissionSummary(pattern.getHost(), info.getContentSetting()));

                preference.setChecked(info.getContentSetting() == ContentSettingValues.ALLOW);
                preference.setOnPreferenceChangeListener(
                        (pref, newValue) -> {
                            @ContentSettingValues
                            int contentSetting =
                                    (boolean) newValue
                                            ? ContentSettingValues.ALLOW
                                            : ContentSettingValues.BLOCK;
                            info.setContentSetting(handle, contentSetting);
                            preference.setSummary(
                                    getEmbeddedPermissionSummary(
                                            pattern.getHost(), contentSetting));
                            preference.setIcon(
                                    getContentSettingsIcon(
                                            info.getContentSettingType(), contentSetting));

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

    private void setUpOsWarningPreferences() {
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        SiteSettingsCategory categoryWithWarning = getWarningCategory();
        // Remove the 'permission is off in Android' message if not needed.
        if (categoryWithWarning == null) {
            removePreferenceSafely(PREF_OS_PERMISSIONS_WARNING);
            removePreferenceSafely(PREF_OS_PERMISSIONS_WARNING_EXTRA);
            removePreferenceSafely(PREF_OS_PERMISSIONS_WARNING_DIVIDER);
        } else {
            Preference osWarning = findPreference(PREF_OS_PERMISSIONS_WARNING);
            Preference osWarningExtra = findPreference(PREF_OS_PERMISSIONS_WARNING_EXTRA);
            categoryWithWarning.configurePermissionIsOffPreferences(
                    osWarning,
                    osWarningExtra,
                    getContext(),
                    false,
                    getSiteSettingsDelegate().getAppName());
            if (osWarning.getTitle() == null) {
                preferenceScreen.removePreference(osWarning);
            } else if (osWarningExtra.getTitle() == null) {
                preferenceScreen.removePreference(osWarningExtra);
            }
        }
    }

    private void setupRelatedSitesPreferences() {
        PreferenceCategory relatedSitesHeader = findPreference(PREF_RELATED_SITES_HEADER);
        TextMessagePreference relatedSitesText = new TextMessagePreference(getContext(), null);
        boolean shouldRelatedSitesPrefBeVisible =
                getSiteSettingsDelegate().isPrivacySandboxFirstPartySetsUIFeatureEnabled()
                        && getSiteSettingsDelegate().isRelatedWebsiteSetsDataAccessEnabled()
                        && mSite.getRWSCookieInfo() != null;
        relatedSitesHeader.setVisible(shouldRelatedSitesPrefBeVisible);
        relatedSitesText.setVisible(shouldRelatedSitesPrefBeVisible);

        if (shouldRelatedSitesPrefBeVisible) {
            var rwsInfo = mSite.getRWSCookieInfo();
            relatedSitesText.setTitle(
                    getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.allsites_rws_summary,
                                    rwsInfo.getMembersCount(),
                                    Integer.toString(rwsInfo.getMembersCount()),
                                    rwsInfo.getOwner()));
            relatedSitesText.setManagedPreferenceDelegate(
                    new ForwardingManagedPreferenceDelegate(
                            getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return getSiteSettingsDelegate()
                                    .isPartOfManagedRelatedWebsiteSet(
                                            mSite.getAddress().getOrigin());
                        }
                    });
            relatedSitesHeader.addPreference(relatedSitesText);

            if (getSiteSettingsDelegate().shouldShowPrivacySandboxRwsUi()) {
                relatedSitesHeader.removeAll();
                relatedSitesHeader.addPreference(relatedSitesText);
                for (Website site : mSite.getRWSCookieInfo().getMembers()) {
                    WebsiteRowPreference preference =
                            new RwsRowPreference(
                                    relatedSitesHeader.getContext(),
                                    getSiteSettingsDelegate(),
                                    site,
                                    getActivity().getLayoutInflater());
                    relatedSitesHeader.addPreference(preference);
                }
            }
        }
    }

    private void setUpAdsInformationalBanner() {
        // Add the informational banner which shows at the top of the UI if ad blocking is
        // activated on this site.
        boolean adBlockingActivated =
                SiteSettingsCategory.adsCategoryEnabled()
                        && WebsitePreferenceBridge.getAdBlockingActivated(
                                getSiteSettingsDelegate().getBrowserContextHandle(),
                                mSite.getAddress().getOrigin())
                        && findPreference(getPreferenceKey(ContentSettingsType.ADS)) != null;

        if (!adBlockingActivated) {
            removePreferenceSafely(PREF_INTRUSIVE_ADS_INFO);
            removePreferenceSafely(PREF_INTRUSIVE_ADS_INFO_DIVIDER);
        }
    }

    private SiteSettingsCategory getWarningCategory() {
        // If more than one per-app permission is disabled in Android, we can pick any category to
        // show the warning, because they will all show the same warning and all take the user to
        // the user to the same location. It is preferrable, however, that we give Geolocation some
        // priority because that category is the only one that potentially shows an additional
        // warning (when Location is turned off globally).
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        if (showWarningFor(SiteSettingsCategory.Type.DEVICE_LOCATION)) {
            return SiteSettingsCategory.createFromType(
                    browserContextHandle, SiteSettingsCategory.Type.DEVICE_LOCATION);
        } else if (showWarningFor(SiteSettingsCategory.Type.CAMERA)) {
            return SiteSettingsCategory.createFromType(
                    browserContextHandle, SiteSettingsCategory.Type.CAMERA);
        } else if (showWarningFor(SiteSettingsCategory.Type.MICROPHONE)) {
            return SiteSettingsCategory.createFromType(
                    browserContextHandle, SiteSettingsCategory.Type.MICROPHONE);
        } else if (showWarningFor(SiteSettingsCategory.Type.NOTIFICATIONS)) {
            return SiteSettingsCategory.createFromType(
                    browserContextHandle, SiteSettingsCategory.Type.NOTIFICATIONS);
        } else if (showWarningFor(SiteSettingsCategory.Type.NFC)) {
            return SiteSettingsCategory.createFromType(
                    browserContextHandle, SiteSettingsCategory.Type.NFC);
        } else if (showWarningFor(SiteSettingsCategory.Type.HAND_TRACKING)) {
            return SiteSettingsCategory.createFromType(
                    browserContextHandle, SiteSettingsCategory.Type.HAND_TRACKING);
        } else if (showWarningFor(SiteSettingsCategory.Type.AUGMENTED_REALITY)) {
            return SiteSettingsCategory.createFromType(
                    browserContextHandle, SiteSettingsCategory.Type.AUGMENTED_REALITY);
        }
        return null;
    }

    private boolean showWarningFor(@SiteSettingsCategory.Type int type) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        @ContentSettingValues
        Integer permission =
                mSite.getContentSetting(
                        browserContextHandle, SiteSettingsCategory.contentSettingsType(type));

        if (permission == null || permission == ContentSettingValues.BLOCK) {
            return false;
        }
        return SiteSettingsCategory.createFromType(browserContextHandle, type)
                .showPermissionBlockedMessage(getContext());
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

    private void setupContentSettingsPreference(
            Preference preference,
            @ContentSettingValues @Nullable Integer value,
            boolean isEmbargoed,
            boolean isOneTime) {
        if (value == null) return;
        setUpPreferenceCommon(preference, value);

        ChromeSwitchPreference switchPreference = (ChromeSwitchPreference) preference;
        switchPreference.setChecked(value == ContentSettingValues.ALLOW);
        switchPreference.setSummary(
                isEmbargoed
                        ? getString(R.string.automatically_blocked)
                        : getString(ContentSettingsResources.getCategorySummary(value, isOneTime)));
        switchPreference.setOnPreferenceChangeListener(this);
        @ContentSettingsType.EnumType
        int contentType = getContentSettingsTypeFromPreferenceKey(preference.getKey());
        if (contentType == mHighlightedPermission) {
            switchPreference.setBackgroundColor(
                    AppCompatResources.getColorStateList(getContext(), mHighlightColor)
                            .getDefaultColor());
        }
    }

    /**
     * Sets some properties that apply to both regular Preferences and ChromeSwitchPreferences, i.e.
     * preference title, enabled-state, and icon, based on the preference's key.
     */
    private void setUpPreferenceCommon(
            Preference preference, @ContentSettingValues @Nullable Integer value) {
        @ContentSettingsType.EnumType
        int contentType = getContentSettingsTypeFromPreferenceKey(preference.getKey());
        int titleResourceId = ContentSettingsResources.getTitle(contentType);

        if (titleResourceId != 0) {
            preference.setTitle(titleResourceId);
        }

        SiteSettingsCategory category =
                SiteSettingsCategory.createFromContentSettingsType(
                        getSiteSettingsDelegate().getBrowserContextHandle(), contentType);
        if (category != null
                && value != null
                && value != ContentSettingValues.BLOCK
                && !category.enabledInAndroid(getActivity())) {
            preference.setIcon(category.getDisabledInAndroidIcon(getContext()));
            preference.setEnabled(false);
        } else {
            preference.setIcon(getContentSettingsIcon(contentType, value));
        }

        // These preferences are persisted elsewhere, using SharedPreferences
        // can cause issues with keys matching up with value.
        preference.setPersistent(false);
        preference.setOrder(++mMaxPermissionOrder);
        getPreferenceScreen().addPreference(preference);
    }

    private void setUpLocationPreference(Preference preference) {
        @ContentSettingValues
        @Nullable
        Integer permission =
                mSite.getContentSetting(
                        getSiteSettingsDelegate().getBrowserContextHandle(),
                        ContentSettingsType.GEOLOCATION);
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

    private void setUpSoundPreference(Preference preference) {
        if (!getArguments().getBoolean(EXTRA_SHOW_SOUND, true)) {
            return;
        }

        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        @ContentSettingValues
        @Nullable
        Integer currentValue =
                mSite.getContentSetting(browserContextHandle, ContentSettingsType.SOUND);
        // In order to always show the sound permission, set it up with the default value if it
        // doesn't have a current value.
        if (currentValue == null) {
            currentValue =
                    WebsitePreferenceBridge.isCategoryEnabled(
                                    browserContextHandle, ContentSettingsType.SOUND)
                            ? ContentSettingValues.ALLOW
                            : ContentSettingValues.BLOCK;
        }
        // Not possible to embargo SOUND.
        setupContentSettingsPreference(
                preference,
                currentValue,
                /* isEmbargoed= */ false,
                isOneTime(ContentSettingsType.SOUND));
    }

    private void setUpJavascriptPreference(Preference preference) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        @ContentSettingValues
        @Nullable
        Integer currentValue =
                mSite.getContentSetting(browserContextHandle, ContentSettingsType.JAVASCRIPT);
        // If Javascript is blocked by default, then always show a Javascript permission.
        // To do this, set it to the default value (blocked).
        if ((currentValue == null)
                && !WebsitePreferenceBridge.isCategoryEnabled(
                        browserContextHandle, ContentSettingsType.JAVASCRIPT)) {
            currentValue = ContentSettingValues.BLOCK;
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
     * has some custom behavior.
     * 1. If the site is a candidate and has activation, the permission should show up even if it
     *    is set as the default (e.g. |preference| is null).
     * 2. The BLOCK string is custom.
     */
    private void setUpAdsPreference(Preference preference) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
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
        @ContentSettingValues
        @Nullable
        Integer permission = mSite.getContentSetting(browserContextHandle, ContentSettingsType.ADS);

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
                            ? ContentSettingValues.ALLOW
                            : ContentSettingValues.BLOCK;
        }
        // Not possible to embargo ADS.
        setupContentSettingsPreference(
                preference,
                permission,
                /* isEmbargoed= */ false,
                isOneTime(ContentSettingsType.ADS));
    }

    private String getDSECategorySummary(@ContentSettingValues int value) {
        return value == ContentSettingValues.ALLOW
                ? getString(R.string.website_settings_permissions_allowed_dse)
                : getString(R.string.website_settings_permissions_blocked_dse);
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
            // Save the paused fragment before finishing the current fragment as it may cause the
            // paused fragment to resume.
            GroupedWebsitesSettings groupFragment = GroupedWebsitesSettings.getPausedInstance();
            getSettingsNavigation().finishCurrentSettings(this);
            if (mFromGrouped && groupFragment != null) {
                getSettingsNavigation().finishCurrentSettings(groupFragment);
            }
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        // It is possible that this UI is destroyed while a dialog is open because
        // incognito mode is closed through the system notification.
        if (getView() == null) return true;
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        int type = getContentSettingsTypeFromPreferenceKey(preference.getKey());
        if (type == ContentSettingsType.DEFAULT) return false;

        @ContentSettingValues int permission;
        if (newValue instanceof Boolean) {
            permission =
                    (Boolean) newValue ? ContentSettingValues.ALLOW : ContentSettingValues.BLOCK;
        } else {
            permission = (Integer) newValue;
        }

        mSite.setContentSetting(browserContextHandle, type, permission);
        // In Clank, one time grants are only possible via prompt, not via page
        // info.
        preference.setSummary(
                getString(ContentSettingsResources.getCategorySummary(permission, false)));
        preference.setIcon(getContentSettingsIcon(type, permission));

        if (mWebsiteSettingsObserver != null) {
            mWebsiteSettingsObserver.onPermissionChanged();
        }

        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
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
                mSite.getTotalUsage() == 0 && !hasManagedChooserPermissions();

        SiteDataCleaner.resetPermissions(
                getSiteSettingsDelegate().getBrowserContextHandle(), mSite);
        SiteDataCleaner.clearData(getSiteSettingsDelegate(), mSite, mDataClearedCallback);

        RecordHistogram.recordEnumeratedHistogram(
                "Privacy.DeleteBrowsingData.Action",
                DeleteBrowsingDataAction.SITES_SETTINGS_PAGE,
                DeleteBrowsingDataAction.MAX_VALUE);
        if (finishActivityImmediately) {
            // Save the paused fragment before finishing the current fragment as it may cause the
            // paused fragment to resume.
            GroupedWebsitesSettings groupFragment = GroupedWebsitesSettings.getPausedInstance();
            getSettingsNavigation().finishCurrentSettings(this);
            if (mFromGrouped && groupFragment != null) {
                getSettingsNavigation().finishCurrentSettings(groupFragment);
            }
        }
    }

    public boolean isOneTime(@ContentSettingsType.EnumType int type) {
        PermissionInfo permissionInfo = mSite.getPermissionInfo(type);
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
                                            getSiteSettingsDelegate().getBrowserContextHandle(),
                                            mSite);
                                    if (mWebsiteSettingsObserver != null) {
                                        mWebsiteSettingsObserver.onPermissionsReset();
                                    }
                                })
                        .setNegativeButton(
                                R.string.cancel, (dialog, which) -> mConfirmationDialog = null)
                        .show();
    }

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
}
