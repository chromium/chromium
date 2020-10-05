// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.text.format.Formatter;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

/**
 * Shows the permissions and other settings for a particular website.
 */
public class SingleWebsiteSettings extends SiteSettingsPreferenceFragment
        implements Preference.OnPreferenceChangeListener, Preference.OnPreferenceClickListener {
    /**
     * Interface for a class that wants to receive updates from SingleWebsiteSettings.
     */
    public interface Observer {
        /**
         * Notifies the observer that the website was reset.
         */
        void onPermissionsReset();

        /**
         * Notifies the observer that a permission was changed.
         */
        void onPermissionChanged();
    }

    // SingleWebsiteSettings expects either EXTRA_SITE (a Website) or
    // EXTRA_SITE_ADDRESS (a WebsiteAddress) to be present (but not both). If
    // EXTRA_SITE is present, the fragment will display the permissions in that
    // Website object. If EXTRA_SITE_ADDRESS is present, the fragment will find all
    // permissions for that website address and display those.
    public static final String EXTRA_SITE = "org.chromium.chrome.preferences.site";
    public static final String EXTRA_SITE_ADDRESS = "org.chromium.chrome.preferences.site_address";

    // Preference keys, see single_website_preferences.xml
    // Headings:
    public static final String PREF_PAGE_DESCRIPTION = "page_description";
    public static final String PREF_SITE_HEADING = "site_heading";
    public static final String PREF_SITE_TITLE = "site_title";
    public static final String PREF_USAGE = "site_usage";
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

    /**
     * @param type ContentSettingsType
     * @return The preference key of this type
     */
    private static @Nullable String getPreferenceKey(@ContentSettingsType int type) {
        switch (type) {
            case ContentSettingsType.ADS:
                return "ads_permission_list";
            case ContentSettingsType.AUTOMATIC_DOWNLOADS:
                return "automatic_downloads_permission_list";
            case ContentSettingsType.BACKGROUND_SYNC:
                return "background_sync_permission_list";
            case ContentSettingsType.BLUETOOTH_SCANNING:
                return "bluetooth_scanning_permission_list";
            case ContentSettingsType.COOKIES:
                return "cookies_permission_list";
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
            PREF_PERMISSIONS_HEADER,
            PREF_CLEAR_DATA,
    };

    // The callback to be run after this site is reset.
    private Observer mWebsiteSettingsObserver;

    private static final int REQUEST_CODE_NOTIFICATION_CHANNEL_SETTINGS = 1;

    private final SiteDataCleaner mSiteDataCleaner = new SiteDataCleaner();

    // The website this page is displaying details about.
    private Website mSite;

    // The Preference key for chooser object permissions.
    private static final String CHOOSER_PERMISSION_PREFERENCE_KEY = "chooser_permission_list";

    // The number of user and policy chosen object permissions displayed.
    private int mObjectUserPermissionCount;
    private int mObjectPolicyPermissionCount;

    // Records previous notification permission on Android O+ to allow detection of permission
    // revocation within the Android system permission activity.
    private @ContentSettingValues @Nullable Integer mPreviousNotificationPermission;

    // Map from preference key to ContentSettingsType.
    private Map<String, Integer> mPreferenceMap;

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

    private final Runnable mDataClearedCallback = () -> {
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
        // Handled in displaySitePermissions. Moving the addPreferencesFromResource call up to here
        // causes animation jank (crbug.com/985734).
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        getActivity().setTitle(R.string.prefs_site_settings);
        init();
        super.onActivityCreated(savedInstanceState);
    }

    private void init() {
        Object extraSite = getArguments().getSerializable(EXTRA_SITE);
        Object extraSiteAddress = getArguments().getSerializable(EXTRA_SITE_ADDRESS);

        if (extraSite != null && extraSiteAddress == null) {
            mSite = (Website) extraSite;
            displaySitePermissions();
        } else if (extraSiteAddress != null && extraSite == null) {
            WebsitePermissionsFetcher fetcher = new WebsitePermissionsFetcher(
                    getSiteSettingsClient().getBrowserContextHandle());
            fetcher.fetchAllPreferences(
                    new SingleWebsitePermissionsPopulator((WebsiteAddress) extraSiteAddress));
        } else {
            assert false : "Exactly one of EXTRA_SITE or EXTRA_SITE_ADDRESS must be provided.";
        }

        setDivider(null);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    @Override
    public void onDisplayPreferenceDialog(Preference preference) {
        if (preference instanceof ClearWebsiteStorage) {
            Callback<Boolean> onDialogClosed = (Boolean confirmed) -> {
                if (confirmed) {
                    mSite.clearAllStoredData(getSiteSettingsClient().getBrowserContextHandle(),
                            mDataClearedCallback::run);
                }
            };
            ClearWebsiteStorageDialog dialogFragment =
                    ClearWebsiteStorageDialog.newInstance(preference, onDialogClosed);
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
        Website merged = new Website(address, null);
        // This loop looks expensive, but the amount of data is likely to be relatively small
        // because most sites have very few permissions.
        for (Website other : websites) {
            if (merged.getContentSettingException(ContentSettingsType.ADS) == null
                    && other.getContentSettingException(ContentSettingsType.ADS) != null
                    && other.compareByAddressTo(merged) == 0) {
                merged.setContentSettingException(ContentSettingsType.ADS,
                        other.getContentSettingException(ContentSettingsType.ADS));
            }
            for (PermissionInfo info : other.getPermissionInfos()) {
                if (merged.getPermissionInfo(info.getContentSettingsType()) == null
                        && permissionInfoIsForTopLevelOrigin(info, origin)) {
                    merged.setPermissionInfo(info);
                }
            }
            if (merged.getLocalStorageInfo() == null && other.getLocalStorageInfo() != null
                    && origin.equals(other.getLocalStorageInfo().getOrigin())) {
                merged.setLocalStorageInfo(other.getLocalStorageInfo());
            }
            for (StorageInfo storageInfo : other.getStorageInfo()) {
                if (host.equals(storageInfo.getHost())) {
                    merged.addStorageInfo(storageInfo);
                }
            }
            for (ChosenObjectInfo objectInfo : other.getChosenObjectInfo()) {
                if (origin.equals(objectInfo.getOrigin())
                        && (objectInfo.getEmbedder() == null
                                || objectInfo.getEmbedder().equals(SITE_WILDCARD))) {
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

            // TODO(crbug.com/763982): Deal with this TODO colony.
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

    /**
     * Updates the permissions displayed in the UI by fetching them from mSite.
     * Must only be called once mSite is set.
     */
    private void displaySitePermissions() {
        if (getPreferenceScreen() != null) {
            getPreferenceScreen().removeAll();
        }
        SettingsUtils.addPreferencesFromResource(this, R.xml.single_website_preferences);

        int maxPermissionOrder = 0;
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        // Iterate over preferences in reverse order because some preferences will be removed during
        // this setup, causing indices of later preferences to change.
        for (int i = preferenceScreen.getPreferenceCount() - 1; i >= 0; i--) {
            Preference preference = preferenceScreen.getPreference(i);
            setUpPreference(preference);
            if (getContentSettingsTypeFromPreferenceKey(preference.getKey())
                    != ContentSettingsType.DEFAULT) {
                maxPermissionOrder = Math.max(maxPermissionOrder, preference.getOrder());
            }
        }
        setUpChosenObjectPreferences(maxPermissionOrder);
        setUpOsWarningPreferences();

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

    private void setUpPreference(Preference preference) {
        if (PREF_SITE_TITLE.equals(preference.getKey())) {
            preference.setTitle(mSite.getTitle());
        } else if (PREF_CLEAR_DATA.equals(preference.getKey())) {
            setUpClearDataPreference((ClearWebsiteStorage) preference);
        } else if (PREF_RESET_SITE.equals(preference.getKey())) {
            setupResetSitePreference(preference);
        } else {
            @ContentSettingsType
            int type = getContentSettingsTypeFromPreferenceKey(preference.getKey());
            if (type != ContentSettingsType.DEFAULT) {
                if (type == ContentSettingsType.ADS) {
                    setUpAdsPreference(preference);
                } else if (type == ContentSettingsType.SOUND) {
                    setUpSoundPreference(preference);
                } else if (type == ContentSettingsType.JAVASCRIPT) {
                    setUpJavascriptPreference(preference);
                } else if (type == ContentSettingsType.GEOLOCATION) {
                    setUpLocationPreference(preference);
                } else if (type == ContentSettingsType.NOTIFICATIONS) {
                    setUpNotificationsPreference(preference, isPermissionEmbargoed(type));
                } else {
                    setUpListPreference(preference,
                            mSite.getContentSetting(
                                    getSiteSettingsClient().getBrowserContextHandle(), type),
                            isPermissionEmbargoed(type));
                }
                return;
            }
        }
    }

    private boolean isPermissionEmbargoed(@ContentSettingsType int type) {
        return mSite.getPermissionInfo(type) != null && mSite.getPermissionInfo(type).isEmbargoed();
    }

    private void setUpClearDataPreference(ClearWebsiteStorage preference) {
        long usage = mSite.getTotalUsage();
        if (usage > 0) {
            boolean appFound = getSiteSettingsClient()
                                       .getWebappSettingsClient()
                                       .getOriginsWithInstalledApp()
                                       .contains(mSite.getAddress().getOrigin());
            Context context = preference.getContext();
            preference.setTitle(
                    String.format(context.getString(R.string.origin_settings_storage_usage_brief),
                            Formatter.formatShortFileSize(context, usage)));
            preference.setDataForDisplay(mSite.getTitle(), appFound);
            if (WebsitePreferenceBridge.isCookieDeletionDisabled(
                        getSiteSettingsClient().getBrowserContextHandle(),
                        mSite.getAddress().getOrigin())) {
                preference.setEnabled(false);
            }
        } else {
            getPreferenceScreen().removePreference(preference);
        }
    }

    private void setupResetSitePreference(Preference preference) {
        int titleResId = mHideNonPermissionPreferences ? R.string.page_info_permissions_reset
                                                       : R.string.website_reset;
        preference.setTitle(titleResId);
        preference.setOnPreferenceClickListener(this);
        if (WebsitePreferenceBridge.isCookieDeletionDisabled(
                    getSiteSettingsClient().getBrowserContextHandle(),
                    mSite.getAddress().getOrigin())) {
            preference.setEnabled(false);
        }
    }

    private Intent getSettingsIntent(String packageName, @ContentSettingsType int type) {
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
    private ChromeImageViewPreference replaceWithReadOnlyCopyOf(
            Preference oldPreference, String newSummary) {
        ChromeImageViewPreference newPreference =
                new ChromeImageViewPreference(oldPreference.getContext());
        newPreference.setKey(oldPreference.getKey());
        setUpPreferenceCommon(newPreference);
        newPreference.setSummary(newSummary);

        // This preference is read-only so should not attempt to persist to shared prefs.
        newPreference.setPersistent(false);

        newPreference.setOrder(oldPreference.getOrder());
        getPreferenceScreen().removePreference(oldPreference);
        getPreferenceScreen().addPreference(newPreference);
        return newPreference;
    }

    private boolean setUpManagedByPreference(Preference preference,
            @StringRes int contentDescriptionRes, @ContentSettingsType int type) {
        Origin origin = Origin.create(mSite.getAddress().getOrigin());
        if (origin == null) {
            return false;
        }

        String managedBy = getSiteSettingsClient().getDelegateAppNameForOrigin(origin, type);
        if (managedBy == null) {
            return false;
        }

        final Intent settingsIntent = getSettingsIntent(
                getSiteSettingsClient().getDelegatePackageNameForOrigin(origin, type), type);
        String summaryText = getString(R.string.website_setting_managed_by_app, managedBy);
        ChromeImageViewPreference newPreference =
                replaceWithReadOnlyCopyOf(preference, summaryText);

        newPreference.setImageView(R.drawable.permission_popups, contentDescriptionRes, null);
        // By disabling the ImageView, clicks will go through to the preference.
        newPreference.setImageViewEnabled(false);

        newPreference.setOnPreferenceClickListener(unused -> {
            startActivity(settingsIntent);
            return true;
        });
        return true;
    }

    private void setUpNotificationsPreference(Preference preference, boolean isEmbargoed) {
        if (setUpManagedByPreference(preference, R.string.website_notification_settings,
                    ContentSettingsType.NOTIFICATIONS)) {
            return;
        }

        final @ContentSettingValues @Nullable Integer value =
                mSite.getContentSetting(getSiteSettingsClient().getBrowserContextHandle(),
                        ContentSettingsType.NOTIFICATIONS);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (value == null
                    || (value != null && value != ContentSettingValues.ALLOW
                            && value != ContentSettingValues.BLOCK)) {
                // TODO(crbug.com/735110): Figure out if this is the correct thing to do, for values
                // that are non-null, but not ALLOW or BLOCK either. (In setupListPreference we
                // treat non-ALLOW settings as BLOCK, but here we are simply removing them.)
                getPreferenceScreen().removePreference(preference);
                return;
            }
            String overrideSummary;
            if (isPermissionControlledByDSE(ContentSettingsType.NOTIFICATIONS)) {
                overrideSummary = getString(value != null && value == ContentSettingValues.ALLOW
                                ? R.string.website_settings_permissions_allow_dse
                                : R.string.website_settings_permissions_block_dse);
            } else {
                overrideSummary = isEmbargoed
                        ? getString(R.string.automatically_blocked)
                        : getString(ContentSettingsResources.getSiteSummary(value));
            }

            // On Android O this preference is read-only, so we replace the existing pref with a
            // regular Preference that takes users to OS settings on click.
            ChromeImageViewPreference newPreference =
                    replaceWithReadOnlyCopyOf(preference, overrideSummary);
            newPreference.setDefaultValue(value);

            newPreference.setOnPreferenceClickListener(unused -> {
                launchOsChannelSettingsFromPreference(preference);
                return true;
            });
        } else {
            setUpListPreference(preference, value, isEmbargoed);
            if (isPermissionControlledByDSE(ContentSettingsType.NOTIFICATIONS) && value != null) {
                updatePreferenceForDSESetting(preference);
            }
        }
    }

    // This is implemented as a public utility function to better facilitate testing.
    public void launchOsChannelSettingsFromPreference(Preference preference) {
        // There is no notification channel if the origin is merely embargoed. Create it
        // just-in-time if the user tries to change to setting.
        if (isPermissionEmbargoed(ContentSettingsType.NOTIFICATIONS)) {
            mSite.setContentSetting(getSiteSettingsClient().getBrowserContextHandle(),
                    ContentSettingsType.NOTIFICATIONS, ContentSettingValues.BLOCK);
        }

        // There is no guarantee that a channel has been initialized yet for sites
        // that were granted permission before the channel-initialization-on-grant
        // code was in place. However, getChannelIdForOrigin will fall back to the
        // generic Sites channel if no specific channel has been created for the given
        // origin, so it is safe to open the channel settings for whatever channel ID
        // it returns.
        String channelId =
                getSiteSettingsClient().getChannelIdForOrigin(mSite.getAddress().getOrigin());
        launchOsChannelSettings(preference.getContext(), channelId);
    }

    private void launchOsChannelSettings(Context context, String channelId) {
        // Store current value of permission to allow comparison against new value at return.
        mPreviousNotificationPermission =
                mSite.getContentSetting(getSiteSettingsClient().getBrowserContextHandle(),
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
            // User has navigated back from system channel settings on O+. Ensure notification
            // preference is up to date, since they might have toggled it from channel settings.
            Preference notificationsPreference =
                    findPreference(getPreferenceKey(ContentSettingsType.NOTIFICATIONS));
            if (notificationsPreference != null) {
                setUpNotificationsPreference(notificationsPreference, false /* isEmbargoed */);
            }

            // To ensure UMA receives notification revocations, we detect if the setting has changed
            // after returning to Chrome.  This is lossy, as it will miss when users revoke a
            // permission, but do not return immediately to Chrome (e.g. they close the permissions
            // activity, instead of hitting the back button), but prevents us from having to check
            // for changes each time Chrome becomes active.
            @ContentSettingValues
            int newPermission =
                    mSite.getContentSetting(getSiteSettingsClient().getBrowserContextHandle(),
                            ContentSettingsType.NOTIFICATIONS);
            if (mPreviousNotificationPermission == ContentSettingValues.ALLOW
                    && newPermission != ContentSettingValues.ALLOW) {
                WebsitePreferenceBridgeJni.get().reportNotificationRevokedForOrigin(
                        getSiteSettingsClient().getBrowserContextHandle(),
                        mSite.getAddress().getOrigin(), newPermission);
                mPreviousNotificationPermission = null;
            }
        }
    }

    /**
     * Creates a ChromeImageViewPreference for each object permission with a
     * ManagedPreferenceDelegate that configures the Preference's widget to display a managed icon
     * and show a toast if a managed permission is clicked. The number of object permissions are
     * tracked by |mObjectPolicyPermissionCount| and |mObjectUserPermissionCount|, which are used
     * when permissions are modified to determine if this preference list should be displayed or
     * not. The preferences are added to the preference screen using |maxPermissionOrder| to order
     * the preferences in the list.
     * @param maxPermissionOrder The listing order of the ChromeImageViewPreference(s) with respect
     *                           to the other preferences.
     */
    private void setUpChosenObjectPreferences(int maxPermissionOrder) {
        PreferenceScreen preferenceScreen = getPreferenceScreen();

        for (ChosenObjectInfo info : mSite.getChosenObjectInfo()) {
            ChromeImageViewPreference preference =
                    new ChromeImageViewPreference(getStyledContext());

            preference.setKey(CHOOSER_PERMISSION_PREFERENCE_KEY);
            preference.setIcon(SettingsUtils.getTintedIcon(getActivity(),
                    ContentSettingsResources.getIcon(info.getContentSettingsType())));
            preference.setOrder(maxPermissionOrder);
            preference.setTitle(info.getName());
            preference.setImageView(R.drawable.ic_delete_white_24dp,
                    R.string.website_settings_revoke_device_permission, (View view) -> {
                        info.revoke(getSiteSettingsClient().getBrowserContextHandle());
                        preferenceScreen.removePreference(preference);
                        mObjectUserPermissionCount--;

                        if (!hasPermissionsPreferences()) {
                            removePreferenceSafely(PREF_PERMISSIONS_HEADER);
                        }
                    });

            preference.setManagedPreferenceDelegate(new ForwardingManagedPreferenceDelegate(
                    getSiteSettingsClient().getManagedPreferenceDelegate()) {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return info.isManaged();
                }
            });

            if (info.isManaged()) {
                mObjectPolicyPermissionCount++;
            } else {
                mObjectUserPermissionCount++;
            }

            preferenceScreen.addPreference(preference);
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
            categoryWithWarning.configurePermissionIsOffPreferences(osWarning, osWarningExtra,
                    getActivity(), false, getSiteSettingsClient().getAppName());
            if (osWarning.getTitle() == null) {
                preferenceScreen.removePreference(osWarning);
            } else if (osWarningExtra.getTitle() == null) {
                preferenceScreen.removePreference(osWarningExtra);
            }
        }
    }

    private void setUpAdsInformationalBanner() {
        // Add the informational banner which shows at the top of the UI if ad blocking is
        // activated on this site.
        boolean adBlockingActivated = SiteSettingsCategory.adsCategoryEnabled()
                && WebsitePreferenceBridge.getAdBlockingActivated(
                        getSiteSettingsClient().getBrowserContextHandle(),
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
                getSiteSettingsClient().getBrowserContextHandle();
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
        } else if (showWarningFor(SiteSettingsCategory.Type.AUGMENTED_REALITY)) {
            return SiteSettingsCategory.createFromType(
                    browserContextHandle, SiteSettingsCategory.Type.AUGMENTED_REALITY);
        }
        return null;
    }

    private boolean showWarningFor(@SiteSettingsCategory.Type int type) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
        @ContentSettingValues
        Integer permission = mSite.getContentSetting(
                browserContextHandle, SiteSettingsCategory.contentSettingsType(type));
        return permission != null
                && SiteSettingsCategory.createFromType(browserContextHandle, type)
                           .showPermissionBlockedMessage(getActivity());
    }

    private boolean hasUsagePreferences() {
        // New actions under the Usage preference category must be listed here so that the category
        // heading can be removed when no actions are shown.
        return findPreference(PREF_CLEAR_DATA) != null;
    }

    private boolean hasPermissionsPreferences() {
        if (mObjectUserPermissionCount > 0 || mObjectPolicyPermissionCount > 0) return true;
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        for (int i = 0; i < preferenceScreen.getPreferenceCount(); i++) {
            String key = preferenceScreen.getPreference(i).getKey();
            if (getContentSettingsTypeFromPreferenceKey(key) != ContentSettingsType.DEFAULT) {
                return true;
            }
        }
        return false;
    }

    /**
     * Initialize a ListPreference with a certain value.
     * @param preference The ListPreference to initialize.
     * @param value The value to initialize it to.
     */
    private void setUpListPreference(Preference preference,
            @ContentSettingValues @Nullable Integer value, boolean isEmbargoed) {
        if (value == null) {
            getPreferenceScreen().removePreference(preference);
            return;
        }
        setUpPreferenceCommon(preference);
        ListPreference listPreference = (ListPreference) preference;

        CharSequence[] keys = new String[2];
        CharSequence[] descriptions = new String[2];
        keys[0] = ContentSetting.toString(ContentSettingValues.ALLOW);
        keys[1] = ContentSetting.toString(ContentSettingValues.BLOCK);
        descriptions[0] =
                getString(ContentSettingsResources.getSiteSummary(ContentSettingValues.ALLOW));
        descriptions[1] =
                getString(ContentSettingsResources.getSiteSummary(ContentSettingValues.BLOCK));
        listPreference.setEntryValues(keys);
        listPreference.setEntries(descriptions);
        listPreference.setOnPreferenceChangeListener(this);
        listPreference.setSummary(isEmbargoed ? getString(R.string.automatically_blocked) : "%s");
        // TODO(crbug.com/735110): Figure out if this is the correct thing to do - here we are
        // effectively treating non-ALLOW values as BLOCK.
        int index = (value == ContentSettingValues.ALLOW ? 0 : 1);
        listPreference.setValueIndex(index);
    }

    /**
     * Sets some properties that apply to both regular Preferences and ListPreferences, i.e.
     * preference title, enabled-state, and icon, based on the preference's key.
     */
    private void setUpPreferenceCommon(Preference preference) {
        int contentType = getContentSettingsTypeFromPreferenceKey(preference.getKey());
        int explanationResourceId = ContentSettingsResources.getExplanation(contentType);
        if (explanationResourceId != 0) {
            preference.setTitle(explanationResourceId);
        }
        if (!preference.isEnabled()) {
            preference.setIcon(
                    ContentSettingsResources.getDisabledIcon(contentType, getResources()));
            return;
        }
        SiteSettingsCategory category = SiteSettingsCategory.createFromContentSettingsType(
                getSiteSettingsClient().getBrowserContextHandle(), contentType);
        if (category != null && !category.enabledInAndroid(getActivity())) {
            preference.setIcon(category.getDisabledInAndroidIcon(getActivity()));
            preference.setEnabled(false);
        } else {
            preference.setIcon(SettingsUtils.getTintedIcon(
                    getActivity(), ContentSettingsResources.getIcon(contentType)));
        }
    }

    private void setUpLocationPreference(Preference preference) {
        if (setUpManagedByPreference(preference, R.string.website_location_settings,
                    ContentSettingsType.GEOLOCATION)) {
            return;
        }

        @ContentSettingValues
        @Nullable
        Integer permission = mSite.getContentSetting(
                getSiteSettingsClient().getBrowserContextHandle(), ContentSettingsType.GEOLOCATION);
        setUpListPreference(
                preference, permission, isPermissionEmbargoed(ContentSettingsType.GEOLOCATION));
        if (isPermissionControlledByDSE(ContentSettingsType.GEOLOCATION) && permission != null) {
            updatePreferenceForDSESetting(preference);
        }
    }

    private void setUpSoundPreference(Preference preference) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
        @ContentSettingValues
        @Nullable
        Integer currentValue =
                mSite.getContentSetting(browserContextHandle, ContentSettingsType.SOUND);
        // In order to always show the sound permission, set it up with the default value if it
        // doesn't have a current value.
        if (currentValue == null) {
            currentValue = WebsitePreferenceBridge.isCategoryEnabled(
                                   browserContextHandle, ContentSettingsType.SOUND)
                    ? ContentSettingValues.ALLOW
                    : ContentSettingValues.BLOCK;
        }
        // Not possible to embargo SOUND.
        setUpListPreference(preference, currentValue, false /* isEmbargoed */);
    }

    private void setUpJavascriptPreference(Preference preference) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
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
        setUpListPreference(preference, currentValue, false /* isEmbargoed */);
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
                getSiteSettingsClient().getBrowserContextHandle();
        // Do not show the setting if the category is not enabled.
        if (!SiteSettingsCategory.adsCategoryEnabled()) {
            setUpListPreference(preference, null, false);
            return;
        }
        // If the ad blocker is activated, then this site will have ads blocked unless there is an
        // explicit permission disallowing the blocking.
        boolean activated = WebsitePreferenceBridge.getAdBlockingActivated(
                browserContextHandle, mSite.getAddress().getOrigin());
        @ContentSettingValues
        @Nullable
        Integer permission = mSite.getContentSetting(browserContextHandle, ContentSettingsType.ADS);

        // If |permission| is null, there is no explicit (non-default) permission set for this site.
        // If the site is not considered a candidate for blocking, do the standard thing and remove
        // the preference.
        if (permission == null && !activated) {
            setUpListPreference(preference, null, false);
            return;
        }

        // However, if the blocking is activated, we still want to show the permission, even if it
        // is in the default state.
        if (permission == null) {
            permission = WebsitePreferenceBridge.isCategoryEnabled(
                                 browserContextHandle, ContentSettingsType.ADS)
                    ? ContentSettingValues.ALLOW
                    : ContentSettingValues.BLOCK;
        }
        // Not possible to embargo ADS.
        setUpListPreference(preference, permission, false /* isEmbargoed */);

        // The subresource filter permission has a custom BLOCK string.
        ListPreference listPreference = (ListPreference) preference;
        listPreference.setEntries(
                new String[] {getString(R.string.website_settings_permissions_allow),
                        getString(R.string.website_settings_permissions_ads_block)});
        listPreference.setValueIndex(permission == ContentSettingValues.ALLOW ? 0 : 1);
    }

    /**
     * Returns true if the DSE (default search engine) geolocation and notifications permissions
     * are configured for the DSE.
     */
    private boolean isPermissionControlledByDSE(@ContentSettingsType int contentSettingsType) {
        return WebsitePreferenceBridge.isPermissionControlledByDSE(
                getSiteSettingsClient().getBrowserContextHandle(), contentSettingsType,
                mSite.getAddress().getOrigin());
    }

    /**
     * Updates the location preference to indicate that the site has access to location (via X-Geo)
     * for searches that happen from the omnibox.
     * @param preference The Location preference to modify.
     */
    private void updatePreferenceForDSESetting(Preference preference) {
        ListPreference listPreference = (ListPreference) preference;
        listPreference.setEntries(new String[] {
                getString(R.string.website_settings_permissions_allow_dse),
                getString(R.string.website_settings_permissions_block_dse),
        });
    }

    public @ContentSettingsType int getContentSettingsTypeFromPreferenceKey(String preferenceKey) {
        if (mPreferenceMap == null) {
            mPreferenceMap = new HashMap<>();
            for (@ContentSettingsType int type = 0; type < ContentSettingsType.NUM_TYPES; type++) {
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
            getActivity().finish();
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        @ContentSettingValues
        int permission = ContentSetting.fromString((String) newValue);
        // Embargoed permission preserves summary. Refresh it manually.
        preference.setSummary("%s");
        BrowserContextHandle browserContextHandle =
                getSiteSettingsClient().getBrowserContextHandle();
        int type = getContentSettingsTypeFromPreferenceKey(preference.getKey());
        if (type != ContentSettingsType.DEFAULT) {
            mSite.setContentSetting(browserContextHandle, type, permission);

            if (mWebsiteSettingsObserver != null) {
                mWebsiteSettingsObserver.onPermissionChanged();
            }
        }

        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        int titleResId = mHideNonPermissionPreferences ? R.string.page_info_permissions_reset
                                                       : R.string.website_reset;
        int confirmationResId = mHideNonPermissionPreferences
                ? R.string.page_info_permissions_reset_confirmation
                : R.string.website_reset_confirmation;
        int buttonResId = mHideNonPermissionPreferences ? R.string.reset : titleResId;
        // Handle the Clear & Reset preference click by showing a confirmation.
        new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                .setTitle(titleResId)
                .setMessage(confirmationResId)
                .setPositiveButton(buttonResId,
                        (dialog, which) -> {
                            if (mHideNonPermissionPreferences) {
                                mSiteDataCleaner.resetPermissions(
                                        getSiteSettingsClient().getBrowserContextHandle(), mSite);
                            } else {
                                resetSite();
                            }
                            if (mWebsiteSettingsObserver != null) {
                                mWebsiteSettingsObserver.onPermissionsReset();
                            }
                        })
                .setNegativeButton(R.string.cancel, null)
                .show();
        return true;
    }

    /**
     * Resets the current site, clearing all permissions and storage used (inc. cookies).
     */
    @VisibleForTesting
    public void resetSite() {
        if (getActivity() == null) return;
        // Clear the screen.
        // TODO(mvanouwerkerk): Refactor this class so that it does not depend on the screen state
        // for its logic. This class should maintain its own data model, and only update the screen
        // after a change is made.
        for (@ContentSettingsType int type = 0; type < ContentSettingsType.NUM_TYPES; type++) {
            String key = getPreferenceKey(type);
            if (key != null) {
                removePreferenceSafely(key);
            }
        }

        // Clearing stored data implies popping back to parent menu if there is nothing left to
        // show. Therefore, we only need to explicitly close the activity if there's no stored data
        // to begin with. The only exception to this is if there are policy managed permissions as
        // those cannot be reset and will always show.
        boolean finishActivityImmediately =
                mSite.getTotalUsage() == 0 && mObjectPolicyPermissionCount == 0;

        mSiteDataCleaner.resetPermissions(getSiteSettingsClient().getBrowserContextHandle(), mSite);
        mSiteDataCleaner.clearData(
                getSiteSettingsClient().getBrowserContextHandle(), mSite, mDataClearedCallback);

        int navigationSource = getArguments().getInt(
                SettingsNavigationSource.EXTRA_KEY, SettingsNavigationSource.OTHER);
        RecordHistogram.recordEnumeratedHistogram("SingleWebsitePreferences.NavigatedFromToReset",
                navigationSource, SettingsNavigationSource.NUM_ENTRIES);

        if (finishActivityImmediately) {
            getActivity().finish();
        }
    }

    /**
     * Ensures preference exists before removing to avoid NPE in
     * {@link PreferenceScreen#removePreference}.
     */
    private void removePreferenceSafely(CharSequence prefKey) {
        Preference preference = findPreference(prefKey);
        if (preference != null) getPreferenceScreen().removePreference(preference);
    }

    /**
     * Removes any user granted chosen object preference(s) from the preference screen.
     */
    private void removeUserChosenObjectPreferences() {
        Preference preference = findPreference(CHOOSER_PERMISSION_PREFERENCE_KEY);
        if (preference != null && !((ChromeImageViewPreference) preference).isManaged()) {
            getPreferenceScreen().removePreference(preference);
        }
        mObjectUserPermissionCount = 0;

        if (mObjectPolicyPermissionCount > 0) {
            ManagedPreferencesUtils.showManagedSettingsCannotBeResetToast(getActivity());
        }
    }
}
