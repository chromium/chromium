// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.settings.SearchUtils.handleSearchNavigation;
import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;
import static org.chromium.components.content_settings.PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED;
import static org.chromium.components.content_settings.PrefNames.ENABLE_QUIET_NOTIFICATION_PERMISSION_UI;
import static org.chromium.components.content_settings.PrefNames.NOTIFICATIONS_VIBRATE_ENABLED;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Browser;
import android.provider.Settings;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.Preference;
import androidx.preference.Preference.OnPreferenceChangeListener;
import androidx.preference.Preference.OnPreferenceClickListener;
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceManager.OnPreferenceTreeClickListener;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.settings.CardPreference;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.FragmentSettingsNavigation;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.settings.SearchUtils;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.AddExceptionPreference.SiteAddedCallback;
import org.chromium.components.browser_ui.site_settings.AutoDarkMetrics.AutoDarkSettingsChangeSource;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.ProviderType;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Shows a list of sites in a particular Site Settings category. For example, this could show all
 * the websites with microphone permissions. When the user selects a site, SingleWebsiteSettings is
 * launched to allow the user to see or modify the settings for that particular website.
 */
@UsedByReflection("site_settings_preferences.xml")
public class SingleCategorySettings extends BaseSiteSettingsFragment
        implements EmbeddableSettingsPage,
                OnPreferenceChangeListener,
                OnPreferenceClickListener,
                SiteAddedCallback,
                OnPreferenceTreeClickListener,
                FragmentSettingsNavigation,
                TriStateCookieSettingsPreference.OnCookiesDetailsRequested,
                CustomDividerFragment,
                WebsitePreference.OnStorageAccessWebsiteDetailsRequested {
    @IntDef({
        GlobalToggleLayout.BINARY_TOGGLE,
        GlobalToggleLayout.TRI_STATE_TOGGLE,
        GlobalToggleLayout.TRI_STATE_COOKIE_TOGGLE
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface GlobalToggleLayout {
        int BINARY_TOGGLE = 0;
        int TRI_STATE_TOGGLE = 1;
        int TRI_STATE_COOKIE_TOGGLE = 2;
    }

    // The key to use to pass which category this preference should display,
    // e.g. Location/Popups/All sites (if blank).
    public static final String EXTRA_CATEGORY = "category";
    public static final String EXTRA_TITLE = "title";
    public static final String POLICY = "policy";

    private SettingsNavigation mSettingsNavigation;

    @Override
    public void setSettingsNavigation(SettingsNavigation settingsNavigation) {
        mSettingsNavigation = settingsNavigation;
    }

    /**
     * If present, the list of websites will be filtered by domain using {@link
     * UrlUtilities#getDomainAndRegistry}.
     */
    public static final String EXTRA_SELECTED_DOMAINS = "selected_domains";

    // The list that contains preferences.
    private RecyclerView mListView;
    // The item for searching the list of items.
    private MenuItem mSearchItem;
    // The Site Settings Category we are showing.
    private SiteSettingsCategory mCategory;
    // If not blank, represents a substring to use to search for site names.
    private String mSearch;
    // Whether to group by allowed/blocked list.
    private boolean mGroupByAllowBlock;
    // Whether the Blocked list should be shown expanded.
    private boolean mBlockListExpanded;
    // Whether the Allowed list should be shown expanded.
    private boolean mAllowListExpanded = true;
    // Whether the Managed list should be shown expanded.
    private boolean mManagedListExpanded;
    // Whether this is the first time this screen is shown.
    private boolean mIsInitialRun = true;
    // The number of sites that are on the Allowed list.
    private int mAllowedSiteCount;
    // Whether tri-state ContentSetting is required.
    private @GlobalToggleLayout int mGlobalToggleLayout = GlobalToggleLayout.BINARY_TOGGLE;
    // The "notifications_quiet_ui" preference to allow hiding/showing it.
    private ChromeBaseCheckBoxPreference mNotificationsQuietUiPref;
    // The three-way settings pref for notification and geolocation permissions.
    private TriStatePermissionPreference mNotificationsTriStatePref;
    private TriStatePermissionPreference mLocationTriStatePref;
    // The "desktop_site_window" preference to allow hiding/showing it.
    private ChromeBaseCheckBoxPreference mDesktopSiteWindowPref;
    private CardPreference mCardPreference;

    @Nullable private Set<String> mSelectedDomains;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCookiesDetailsRequested(@CookieControlsMode int cookieSettingsState) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(RWSCookieSettings.EXTRA_COOKIE_PAGE_STATE, cookieSettingsState);

        mSettingsNavigation.startSettings(getActivity(), RWSCookieSettings.class, fragmentArgs);
    }

    @Override
    public void onStorageAccessWebsiteDetailsRequested(WebsitePreference website) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(
                StorageAccessSubpageSettings.EXTRA_STORAGE_ACCESS_STATE, website.site());
        fragmentArgs.putBoolean(
                StorageAccessSubpageSettings.EXTRA_ALLOWED, !isOnBlockList(website));

        mSettingsNavigation.startSettings(
                getActivity(), StorageAccessSubpageSettings.class, fragmentArgs);
    }

    // Note: these values must match the SiteLayout enum in enums.xml.
    @IntDef({SiteLayout.MOBILE, SiteLayout.DESKTOP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SiteLayout {
        int MOBILE = 0;
        int DESKTOP = 1;
        int NUM_ENTRIES = 2;
    }

    // Keys for common ContentSetting toggle for categories. These toggles are mutually
    // exclusive: a category should only show one of them, at most.

    public static final String CARD_PREFERENCE_KEY = "card_preference";
    public static final String BINARY_TOGGLE_KEY = "binary_toggle";
    public static final String TRI_STATE_TOGGLE_KEY = "tri_state_toggle";
    public static final String TRI_STATE_COOKIE_TOGGLE = "tri_state_cookie_toggle";

    // Keys for category-specific preferences (toggle, link, button etc.), dynamically shown.
    public static final String NOTIFICATIONS_VIBRATE_TOGGLE_KEY = "notifications_vibrate";
    public static final String NOTIFICATIONS_QUIET_UI_TOGGLE_KEY = "notifications_quiet_ui";
    public static final String NOTIFICATIONS_TRI_STATE_PREF_KEY = "notifications_tri_state_toggle";
    public static final String LOCATION_TRI_STATE_PREF_KEY = "location_tri_state_toggle";
    public static final String DESKTOP_SITE_WINDOW_TOGGLE_KEY = "desktop_site_window";
    public static final String EXPLAIN_PROTECTED_MEDIA_KEY = "protected_content_learn_more";
    public static final String ADD_EXCEPTION_KEY = "add_exception";
    public static final String INFO_TEXT_KEY = "info_text";
    public static final String ANTI_ABUSE_WHEN_ON_HEADER = "anti_abuse_when_on_header";
    public static final String ANTI_ABUSE_WHEN_ON_SECTION_ONE = "anti_abuse_when_on_section_one";
    public static final String ANTI_ABUSE_WHEN_ON_SECTION_TWO = "anti_abuse_when_on_section_two";
    public static final String ANTI_ABUSE_WHEN_ON_SECTION_THREE =
            "anti_abuse_when_on_section_three";
    public static final String ANTI_ABUSE_THINGS_TO_CONSIDER_HEADER =
            "anti_abuse_things_to_consider_header";
    public static final String ANTI_ABUSE_THINGS_TO_CONSIDER_SECTION_ONE =
            "anti_abuse_things_to_consider_section_one";

    // Keys for Allowed/Blocked preference groups/headers.
    public static final String ALLOWED_GROUP = "allowed_group";
    public static final String BLOCKED_GROUP = "blocked_group";
    public static final String MANAGED_GROUP = "managed_group";

    private class ResultsPopulator implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            // This method may be called after the activity has been destroyed.
            // In that case, bail out.
            if (getActivity() == null) return;

            resetList();

            sites = applyFilters(sites);

            int chooserDataType = mCategory.getObjectChooserDataType();
            if (chooserDataType == -1) {
                addWebsites(sites);
            } else {
                addChosenObjects(sites);
            }
        }

        private Collection<Website> applyFilters(Collection<Website> sites) {
            @SiteSettingsCategory.Type int type = mCategory.getType();
            if (type == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES
                    || type == SiteSettingsCategory.Type.SITE_DATA) {
                Collection<Website> filtered = new ArrayList<>();
                boolean isThirdPartyCategory =
                        type == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES;
                for (Website site : sites) {
                    if (site.representsThirdPartiesOnSite() == isThirdPartyCategory) {
                        filtered.add(site);
                    }
                }
                return filtered;
            }
            return sites;
        }
    }

    /** Called by common settings code to determine if a Preference is managed. */
    private class SingleCategoryManagedPreferenceDelegate
            extends ForwardingManagedPreferenceDelegate {
        SingleCategoryManagedPreferenceDelegate(ManagedPreferenceDelegate base) {
            super(base);
        }

        @Override
        public boolean isPreferenceControlledByPolicy(Preference preference) {
            // TODO(bauerb): Align the ManagedPreferenceDelegate and
            // SiteSettingsCategory interfaces better to avoid this indirection.
            return mCategory.isManaged() && !mCategory.isManagedByCustodian();
        }

        @Override
        public boolean isPreferenceControlledByCustodian(Preference preference) {
            return mCategory.isManagedByCustodian();
        }
    }

    private void getInfoForOrigins() {
        if (!mCategory.enabledInAndroid(getActivity())) {
            // No need to fetch any data if we're not going to show it, but we do need to update
            // the global toggle to reflect updates in Android settings (e.g. Location).
            resetList();
            return;
        }

        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(getSiteSettingsDelegate(), false);
        fetcher.fetchPreferencesForCategory(mCategory, new ResultsPopulator());
    }

    /**
     * Returns whether a website is on the Blocked list for the category currently showing.
     *
     * @param website The website to check.
     */
    private boolean isOnBlockList(WebsitePreference website) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        @ContentSettingValues
        Integer contentSetting =
                website.site()
                        .getContentSetting(
                                browserContextHandle, mCategory.getContentSettingsType());
        if (contentSetting != null) {
            return ContentSettingValues.BLOCK == contentSetting;
        }
        return false;
    }

    /**
     * Update the Category Header for the Allowed list.
     *
     * @param numAllowed The number of sites that are on the Allowed list
     * @param toggleValue The value the global toggle will have once precessing ends.
     */
    private void updateAllowedHeader(int numAllowed, boolean toggleValue) {
        ExpandablePreferenceGroup allowedGroup =
                getPreferenceScreen().findPreference(ALLOWED_GROUP);
        if (allowedGroup == null) return;

        if (numAllowed == 0) {
            if (allowedGroup != null) getPreferenceScreen().removePreference(allowedGroup);
            return;
        }
        if (!mGroupByAllowBlock) return;

        int resourceId;
        if (mCategory.getType() == SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE) {
            // REQUEST_DESKTOP_SITE has its own Allowed list header.
            resourceId = R.string.website_settings_allowed_group_heading_request_desktop_site;
        } else if (toggleValue) {
            resourceId = R.string.website_settings_allowed_group_heading;
        } else {
            // When the toggle is set to Blocked, the Allowed list header should read 'Exceptions',
            // not 'Allowed' (because it shows exceptions from the rule).
            resourceId = R.string.website_settings_exceptions_group_heading;
        }
        allowedGroup.setTitle(getHeaderTitle(resourceId, numAllowed));
        allowedGroup.setExpanded(mAllowListExpanded);
    }

    private void updateBlockedHeader(int numBlocked) {
        ExpandablePreferenceGroup blockedGroup =
                getPreferenceScreen().findPreference(BLOCKED_GROUP);
        if (numBlocked == 0) {
            if (blockedGroup != null) getPreferenceScreen().removePreference(blockedGroup);
            return;
        }
        if (!mGroupByAllowBlock) return;

        // Set the title and arrow icons for the header.
        int resourceId;
        if (mCategory.getType() == SiteSettingsCategory.Type.SOUND) {
            resourceId = R.string.website_settings_blocked_group_heading_sound;
        } else if (mCategory.getType() == SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE) {
            resourceId = R.string.website_settings_blocked_group_heading_request_desktop_site;
        } else {
            resourceId = R.string.website_settings_blocked_group_heading;
        }
        blockedGroup.setTitle(getHeaderTitle(resourceId, numBlocked));
        blockedGroup.setExpanded(mBlockListExpanded);
    }

    private void updateManagedHeader(int numManaged) {
        ExpandablePreferenceGroup managedGroup =
                getPreferenceScreen().findPreference(MANAGED_GROUP);
        if (numManaged == 0) {
            if (managedGroup != null) getPreferenceScreen().removePreference(managedGroup);
            return;
        }
        if (!mGroupByAllowBlock) return;

        // Set the title and arrow icons for the header.
        int resourceId = R.string.website_settings_managed_group_heading;
        managedGroup.setTitle(getHeaderTitle(resourceId, numManaged));
        managedGroup.setExpanded(mManagedListExpanded);
    }

    private CharSequence getHeaderTitle(int resourceId, int count) {
        SpannableStringBuilder spannable = new SpannableStringBuilder(getString(resourceId));
        String prefCount = String.format(Locale.getDefault(), " - %d", count);
        spannable.append(prefCount);

        // Color the first part of the title blue.
        ForegroundColorSpan blueSpan =
                new ForegroundColorSpan(
                        SemanticColorUtils.getDefaultTextColorAccent1(getContext()));
        spannable.setSpan(
                blueSpan,
                0,
                spannable.length() - prefCount.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        // Gray out the total count of items.
        final @ColorInt int gray = SemanticColorUtils.getDefaultTextColorSecondary(getContext());
        spannable.setSpan(
                new ForegroundColorSpan(gray),
                spannable.length() - prefCount.length(),
                spannable.length(),
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        return spannable;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Read which category we should be showing.
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        if (getArguments() != null) {
            mCategory =
                    SiteSettingsCategory.createFromPreferenceKey(
                            browserContextHandle, getArguments().getString(EXTRA_CATEGORY, ""));
        }

        if (mCategory.getType() == SiteSettingsCategory.Type.ALL_SITES
                || mCategory.getType() == SiteSettingsCategory.Type.USE_STORAGE
                || mCategory.getType() == SiteSettingsCategory.Type.ZOOM) {
            throw new IllegalArgumentException("Use AllSiteSettings instead.");
        }

        int contentType = mCategory.getContentSettingsType();
        if (mCategory.getType() == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES) {
            mGlobalToggleLayout = GlobalToggleLayout.TRI_STATE_COOKIE_TOGGLE;
        } else if (WebsitePreferenceBridge.requiresTriStateContentSetting(contentType)) {
            mGlobalToggleLayout = GlobalToggleLayout.TRI_STATE_TOGGLE;
        }

        ViewGroup view = (ViewGroup) super.onCreateView(inflater, container, savedInstanceState);

        mListView = getListView();

        // Disable animations of preference changes.
        mListView.setItemAnimator(null);

        return view;
    }

    @Override
    public boolean hasDivider() {
        // Remove dividers between preferences.
        return false;
    }

    /** Returns the category being displayed. For testing. */
    public SiteSettingsCategory getCategoryForTest() {
        return mCategory;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        // Handled in onActivityCreated. Moving the addPreferencesFromResource call up to here
        // causes animation jank (crbug.com/985734).
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.website_preferences);

        String title = getArguments().getString(EXTRA_TITLE);
        if (title != null) mPageTitle.set(title);

        mSelectedDomains =
                getArguments().containsKey(EXTRA_SELECTED_DOMAINS)
                        ? new HashSet<>(getArguments().getStringArrayList(EXTRA_SELECTED_DOMAINS))
                        : null;

        configureGlobalToggles();
        if (mCategory.getType() == SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE) {
            RecordUserAction.record("DesktopSiteContentSetting.SettingsPage.Entered");
            getSiteSettingsDelegate().notifyRequestDesktopSiteSettingsPageOpened();
        }

        setHasOptionsMenu(true);

        super.onActivityCreated(savedInstanceState);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        inflater.inflate(R.menu.website_preferences_menu, menu);

        mSearchItem = menu.findItem(R.id.search);
        SearchUtils.initializeSearchView(
                mSearchItem,
                mSearch,
                getActivity(),
                (query) -> {
                    boolean queryHasChanged =
                            mSearch == null
                                    ? query != null && !query.isEmpty()
                                    : !mSearch.equals(query);
                    mSearch = query;
                    if (queryHasChanged) getInfoForOrigins();
                });

        if (getSiteSettingsDelegate().isHelpAndFeedbackEnabled()) {
            MenuItem help =
                    menu.add(
                            Menu.NONE,
                            R.id.menu_id_site_settings_help,
                            Menu.NONE,
                            R.string.menu_help);
            help.setIcon(
                    TraceEventVectorDrawableCompat.create(
                            getResources(),
                            R.drawable.ic_help_and_feedback,
                            getContext().getTheme()));
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_site_settings_help) {
            if (mCategory.getType() == SiteSettingsCategory.Type.PROTECTED_MEDIA) {
                getSiteSettingsDelegate()
                        .launchProtectedContentHelpAndFeedbackActivity(getActivity());
            } else {
                getSiteSettingsDelegate().launchSettingsHelpAndFeedbackActivity(getActivity());
            }
            return true;
        }
        if (handleSearchNavigation(item, mSearchItem, mSearch, getActivity())) {
            boolean queryHasChanged = mSearch != null && !mSearch.isEmpty();
            mSearch = null;
            if (queryHasChanged) getInfoForOrigins();
            return true;
        }
        return false;
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        // Do not show the toast if the System Location setting is disabled.
        if (getPreferenceScreen().findPreference(BINARY_TOGGLE_KEY) != null
                && mCategory.isManaged()) {
            showManagedToast();
            return false;
        }

        if (preference instanceof WebsitePreference) {
            WebsitePreference websitePreference = (WebsitePreference) preference;
            if (websitePreference.isManaged()) {
                showManagedToast();
                return false;
            }

            if (websitePreference.getParent().getKey().equals(MANAGED_GROUP)) {
                websitePreference.setFragment(SingleWebsiteSettings.class.getName());
                websitePreference.putSiteAddressIntoExtras(
                        SingleWebsiteSettings.EXTRA_SITE_ADDRESS);
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                    && mCategory.getType() == SiteSettingsCategory.Type.NOTIFICATIONS) {
                // In  Android O+, users can manage Notification channels through App Info. If this
                // is the case we send the user directly to Android Settings to modify the
                // Notification exception.
                String channelId =
                        getSiteSettingsDelegate()
                                .getChannelIdForOrigin(
                                        websitePreference.site().getAddress().getOrigin());
                Intent intent = new Intent(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS);
                intent.putExtra(Settings.EXTRA_CHANNEL_ID, channelId);
                intent.putExtra(
                        Settings.EXTRA_APP_PACKAGE, preference.getContext().getPackageName());
                startActivityForResult(
                        intent, SingleWebsiteSettings.REQUEST_CODE_NOTIFICATION_CHANNEL_SETTINGS);

            } else {
                buildPreferenceDialog(websitePreference.site()).show();
                if (mCategory.getType() == SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE) {
                    RecordUserAction.record(
                            "DesktopSiteContentSetting.SettingsPage.SiteException.Opened");
                }
            }
        }

        return super.onPreferenceTreeClick(preference);
    }

    // OnPreferenceChangeListener:
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        PrefService prefService = UserPrefs.get(browserContextHandle);
        if (BINARY_TOGGLE_KEY.equals(preference.getKey())) {
            assert !mCategory.isManaged();
            boolean toggleValue = (boolean) newValue;

            @SiteSettingsCategory.Type int type = mCategory.getType();
            if (type == SiteSettingsCategory.Type.SITE_DATA && !toggleValue) {
                showDisableSiteDataConfirmationDialog();
                return false;
            }
            WebsitePreferenceBridge.setCategoryEnabled(
                    browserContextHandle,
                    SiteSettingsCategory.contentSettingsType(type),
                    toggleValue);

            if (type == SiteSettingsCategory.Type.NOTIFICATIONS) {
                updateNotificationsSecondaryControls();
            } else if (type == SiteSettingsCategory.Type.DEVICE_LOCATION) {
                updateLocationSecondaryControls();
            } else if (type == SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT) {
                AutoDarkMetrics.recordAutoDarkSettingsChangeSource(
                        AutoDarkSettingsChangeSource.SITE_SETTINGS_GLOBAL, toggleValue);
            } else if (type == SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE) {
                recordSiteLayoutChanged(toggleValue);
                updateDesktopSiteWindowSetting();
            }
            getInfoForOrigins();
        } else if (TRI_STATE_TOGGLE_KEY.equals(preference.getKey())) {
            @ContentSettingValues int setting = (int) newValue;
            WebsitePreferenceBridge.setDefaultContentSetting(
                    browserContextHandle, mCategory.getContentSettingsType(), setting);
            getInfoForOrigins();
        } else if (TRI_STATE_COOKIE_TOGGLE.equals(preference.getKey())) {
            setThirdPartyCookieSettingsPreference((int) newValue);
            getInfoForOrigins();
        } else if (NOTIFICATIONS_VIBRATE_TOGGLE_KEY.equals(preference.getKey())) {
            prefService.setBoolean(NOTIFICATIONS_VIBRATE_ENABLED, (boolean) newValue);
        } else if (NOTIFICATIONS_QUIET_UI_TOGGLE_KEY.equals(preference.getKey())) {
            if ((boolean) newValue) {
                prefService.setBoolean(ENABLE_QUIET_NOTIFICATION_PERMISSION_UI, true);
            } else {
                // Clear the pref so if the default changes later the user will get the new default.
                prefService.clearPref(ENABLE_QUIET_NOTIFICATION_PERMISSION_UI);
            }
        } else if (DESKTOP_SITE_WINDOW_TOGGLE_KEY.equals(preference.getKey())) {
            prefService.setBoolean(DESKTOP_SITE_WINDOW_SETTING_ENABLED, (boolean) newValue);
            RecordHistogram.recordBooleanHistogram(
                    "Android.RequestDesktopSite.WindowSettingChanged", (boolean) newValue);
        }
        return true;
    }

    private void showDisableSiteDataConfirmationDialog() {
        assert mCategory.getType() == SiteSettingsCategory.Type.SITE_DATA;

        var manager =
                new ModalDialogManager(new AppModalPresenter(getContext()), ModalDialogType.APP);
        var controller =
                new Controller() {
                    @Override
                    public void onClick(PropertyModel model, @ButtonType int buttonType) {
                        switch (buttonType) {
                            case ButtonType.POSITIVE:
                                WebsitePreferenceBridge.setCategoryEnabled(
                                        getSiteSettingsDelegate().getBrowserContextHandle(),
                                        mCategory.getContentSettingsType(),
                                        false);
                                getInfoForOrigins();
                                manager.dismissDialog(
                                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                                break;
                            case ButtonType.NEGATIVE:
                                manager.dismissDialog(
                                        model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                                break;
                            default:
                                assert false;
                                break;
                        }
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {}
                };
        var resources = getContext().getResources();
        var builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                resources,
                                R.string.website_settings_site_data_page_block_confirm_dialog_title)
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                resources.getString(
                                        R.string
                                                .website_settings_site_data_page_block_confirm_dialog_description))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string
                                        .website_settings_site_data_page_block_confirm_dialog_confirm_button)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string
                                        .website_settings_site_data_page_block_confirm_dialog_cancel_button);
        var model = builder.build();
        manager.showDialog(model, ModalDialogType.APP);
    }

    private void setThirdPartyCookieSettingsPreference(@CookieControlsMode int mode) {
        assert mCategory.getType() == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES;
        getSiteSettingsDelegate().dismissPrivacySandboxSnackbar();

        // Display the Privacy Sandbox snackbar whenever third-party cookies are blocked.
        if (mode == CookieControlsMode.BLOCK_THIRD_PARTY) {
            getSiteSettingsDelegate().maybeDisplayPrivacySandboxSnackbar();
        }
        PrefService prefService =
                UserPrefs.get(getSiteSettingsDelegate().getBrowserContextHandle());
        prefService.setInteger(COOKIE_CONTROLS_MODE, mode);
    }

    private String getAddExceptionDialogMessage() {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        int resource = 0;
        switch (mCategory.getType()) {
            case SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS:
                resource = R.string.website_settings_add_site_description_automatic_downloads;
                break;
            case SiteSettingsCategory.Type.BACKGROUND_SYNC:
                resource = R.string.website_settings_add_site_description_background_sync;
                break;
            case SiteSettingsCategory.Type.JAVASCRIPT:
                resource =
                        WebsitePreferenceBridge.isCategoryEnabled(
                                        browserContextHandle, ContentSettingsType.JAVASCRIPT)
                                ? R.string.website_settings_add_site_description_javascript_block
                                : R.string.website_settings_add_site_description_javascript_allow;
                break;
            case SiteSettingsCategory.Type.SOUND:
                resource =
                        WebsitePreferenceBridge.isCategoryEnabled(
                                        browserContextHandle, ContentSettingsType.SOUND)
                                ? R.string.website_settings_add_site_description_sound_block
                                : R.string.website_settings_add_site_description_sound_allow;
                break;
            case SiteSettingsCategory.Type.SITE_DATA:
                resource =
                        WebsitePreferenceBridge.isCategoryEnabled(
                                        browserContextHandle, ContentSettingsType.COOKIES)
                                ? R.string
                                        .website_settings_site_data_page_add_block_exception_description
                                : R.string
                                        .website_settings_site_data_page_add_allow_exception_description;
                break;
            case SiteSettingsCategory.Type.THIRD_PARTY_COOKIES:
                resource =
                        getCookieControlsMode() == CookieControlsMode.OFF
                                ? R.string
                                        .website_settings_third_party_cookies_page_add_block_exception_description
                                : R.string
                                        .website_settings_third_party_cookies_page_add_allow_exception_description;
                break;
            case SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT:
                assert WebsitePreferenceBridge.isCategoryEnabled(
                        browserContextHandle, ContentSettingsType.AUTO_DARK_WEB_CONTENT);
                resource = R.string.website_settings_add_site_description_auto_dark_block;
                break;
            case SiteSettingsCategory.Type.FEDERATED_IDENTITY_API:
                resource =
                        WebsitePreferenceBridge.isCategoryEnabled(
                                        browserContextHandle,
                                        ContentSettingsType.FEDERATED_IDENTITY_API)
                                ? R.string
                                        .website_settings_add_site_description_federated_identity_block
                                : R.string
                                        .website_settings_add_site_description_federated_identity_allow;
                break;
            case SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE:
                resource =
                        WebsitePreferenceBridge.isCategoryEnabled(
                                        browserContextHandle,
                                        ContentSettingsType.REQUEST_DESKTOP_SITE)
                                ? R.string
                                        .website_settings_blocked_group_heading_request_desktop_site
                                : R.string
                                        .website_settings_allowed_group_heading_request_desktop_site;
                break;
        }
        assert resource > 0;
        return getString(resource);
    }

    // OnPreferenceClickListener:
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (ALLOWED_GROUP.equals(preference.getKey())) {
            mAllowListExpanded = !mAllowListExpanded;
        } else if (BLOCKED_GROUP.equals(preference.getKey())) {
            mBlockListExpanded = !mBlockListExpanded;
        } else {
            mManagedListExpanded = !mManagedListExpanded;
        }
        getInfoForOrigins();
        return true;
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mSearch == null && mSearchItem != null) {
            SearchUtils.clearSearch(mSearchItem, getActivity());
            mSearch = null;
        }

        getInfoForOrigins();
    }

    // AddExceptionPreference.SiteAddedCallback:
    @Override
    public void onAddSite(String primaryPattern, String secondaryPattern) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        int setting = ContentSettingValues.DEFAULT;
        switch (mGlobalToggleLayout) {
            case GlobalToggleLayout.TRI_STATE_COOKIE_TOGGLE:
                setting =
                        getCookieControlsMode() == CookieControlsMode.OFF
                                ? ContentSettingValues.BLOCK
                                : ContentSettingValues.ALLOW;
                break;
            case GlobalToggleLayout.TRI_STATE_TOGGLE:
            case GlobalToggleLayout.BINARY_TOGGLE:
                setting =
                        WebsitePreferenceBridge.isCategoryEnabled(
                                        browserContextHandle, mCategory.getContentSettingsType())
                                ? ContentSettingValues.BLOCK
                                : ContentSettingValues.ALLOW;
                break;
            default:
                assert false;
        }

        WebsitePreferenceBridge.setContentSettingCustomScope(
                browserContextHandle,
                mCategory.getContentSettingsType(),
                primaryPattern,
                secondaryPattern,
                setting);

        String hostname = primaryPattern.equals(SITE_WILDCARD) ? secondaryPattern : primaryPattern;
        Toast.makeText(
                        getContext(),
                        getContext().getString(R.string.website_settings_add_site_toast, hostname),
                        Toast.LENGTH_SHORT)
                .show();

        getInfoForOrigins();

        if (mCategory.getType() == SiteSettingsCategory.Type.SOUND) {
            if (setting == ContentSettingValues.BLOCK) {
                RecordUserAction.record("SoundContentSetting.MuteBy.PatternException");
            } else {
                RecordUserAction.record("SoundContentSetting.UnmuteBy.PatternException");
            }
        }
        DesktopSiteMetrics.recordDesktopSiteSettingsManuallyAdded(
                mCategory.getType(), setting, hostname);
    }

    /** Reset the preference screen an initialize it again. */
    private void resetList() {
        // This will remove the combo box at the top and all the sites listed below it.
        getPreferenceScreen().removeAll();
        // And this will add the filter preference back (combo box).
        SettingsUtils.addPreferencesFromResource(this, R.xml.website_preferences);

        configureGlobalToggles();

        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        @ContentSettingsType.EnumType int type = mCategory.getContentSettingsType();
        boolean allowSpecifyingExceptions = false;

        switch (mCategory.getType()) {
            case SiteSettingsCategory.Type.SOUND:
            case SiteSettingsCategory.Type.JAVASCRIPT:
            case SiteSettingsCategory.Type.SITE_DATA:
            case SiteSettingsCategory.Type.FEDERATED_IDENTITY_API:
            case SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE:
                allowSpecifyingExceptions = true;
                break;
            case SiteSettingsCategory.Type.BACKGROUND_SYNC:
            case SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS:
                allowSpecifyingExceptions =
                        !WebsitePreferenceBridge.isCategoryEnabled(browserContextHandle, type);
                break;
            case SiteSettingsCategory.Type.THIRD_PARTY_COOKIES:
                allowSpecifyingExceptions = getCookieControlsMode() != CookieControlsMode.OFF;
                break;
            default:
                break;
        }
        if (allowSpecifyingExceptions) {
            getPreferenceScreen()
                    .addPreference(
                            new AddExceptionPreference(
                                    getStyledContext(),
                                    ADD_EXCEPTION_KEY,
                                    getAddExceptionDialogMessage(),
                                    mCategory,
                                    this));
        }
    }

    private boolean addWebsites(Collection<Website> sites) {
        filterSelectedDomains(sites);

        List<WebsitePreference> websites = new ArrayList<>();
        ForwardingManagedPreferenceDelegate websiteDelegate = createWebsiteManagedPrefDelegate();

        // Find origins matching the current search.
        // Check if the source of the exception for each website is a policy
        // to set the managed state needed for the UI.
        for (Website site : sites) {
            if (mSearch == null || mSearch.isEmpty() || site.getTitle().contains(mSearch)) {
                WebsitePreference preference =
                        new WebsitePreference(
                                getStyledContext(), getSiteSettingsDelegate(), site, mCategory);

                if (mCategory.getType() == SiteSettingsCategory.Type.STORAGE_ACCESS) {
                    preference.setStorageAccessSettingsPageListener(this);
                }
                websites.add(preference);
                preference.setManagedPreferenceDelegate(websiteDelegate);
            }
        }

        mAllowedSiteCount = 0;

        if (websites.size() == 0 || !shouldAddExceptionsForCategory()) {
            updateBlockedHeader(0);
            updateAllowedHeader(0, true);
            updateManagedHeader(0);
            return false;
        }

        Collections.sort(websites);
        int blocked = 0;
        int managed = 0;

        if (!mGroupByAllowBlock) {
            // We're not grouping sites into Allowed/Blocked lists, so show all in order
            // (will be alphabetical).
            for (WebsitePreference website : websites) {
                getPreferenceScreen().addPreference(website);
            }
        } else {
            // Group sites into Allowed/Blocked lists.
            PreferenceGroup allowedGroup = getPreferenceScreen().findPreference(ALLOWED_GROUP);
            PreferenceGroup blockedGroup = getPreferenceScreen().findPreference(BLOCKED_GROUP);
            PreferenceGroup managedGroup = getPreferenceScreen().findPreference(MANAGED_GROUP);

            Set<String> delegatedOrigins =
                    mCategory.getType() == SiteSettingsCategory.Type.NOTIFICATIONS
                            ? getSiteSettingsDelegate().getAllDelegatedNotificationOrigins()
                            : Collections.emptySet();

            for (WebsitePreference website : websites) {
                if (delegatedOrigins.contains(website.site().getAddress().getOrigin())) {
                    managedGroup.addPreference(website);
                    managed += 1;
                } else if (isOnBlockList(website)) {
                    blockedGroup.addPreference(website);
                    blocked += 1;
                } else {
                    allowedGroup.addPreference(website);
                    mAllowedSiteCount += 1;
                }
            }

            // For the ads permission, the Allowed list should appear first. Default
            // collapsed settings should not change.
            if (mCategory.getType() == SiteSettingsCategory.Type.ADS) {
                blockedGroup.setOrder(allowedGroup.getOrder() + 1);
            }

            // The default, when the lists are shown for the first time, is for the
            // Blocked and Managed list to be collapsed and Allowed expanded -- because
            // the data in the Allowed list is normally more useful than the data in
            // the Blocked/Managed lists. A collapsed initial Blocked/Managed list works
            // well *except* when there's nothing in the Allowed list because then
            // there's only Blocked/Managed items to show and it doesn't make sense for
            // those items to be hidden. So, in those cases (and only when the lists are
            // shown for the first time) do we ignore the collapsed directive. The user
            // can still collapse and expand the Blocked/Managed list at will.
            if (mIsInitialRun) {
                if (mAllowedSiteCount == 0) {
                    if (blocked == 0 && managed > 0) {
                        mManagedListExpanded = true;
                    } else {
                        mBlockListExpanded = true;
                    }
                }
                mIsInitialRun = false;
            }

            if (!mBlockListExpanded) blockedGroup.removeAll();
            if (!mAllowListExpanded) allowedGroup.removeAll();
            if (!mManagedListExpanded) managedGroup.removeAll();
        }

        updateBlockedHeader(blocked);
        updateAllowedHeader(mAllowedSiteCount, !isBlocked());
        updateManagedHeader(managed);

        return websites.size() != 0;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    private void filterSelectedDomains(Collection<Website> websites) {
        if (mSelectedDomains == null) {
            return;
        }
        for (Iterator<Website> it = websites.iterator(); it.hasNext(); ) {
            String domain =
                    UrlUtilities.getDomainAndRegistry(it.next().getAddress().getOrigin(), true);
            if (!mSelectedDomains.contains(domain)) {
                it.remove();
            }
        }
    }

    private boolean addChosenObjects(Collection<Website> sites) {
        Map<String, Pair<ArrayList<ChosenObjectInfo>, ArrayList<Website>>> objects =
                new HashMap<>();

        // Find chosen objects matching the current search and collect the list of sites
        // that have permission to access each.
        for (Website site : sites) {
            for (ChosenObjectInfo info : site.getChosenObjectInfo()) {
                if (mSearch == null
                        || mSearch.isEmpty()
                        || info.getName().toLowerCase(Locale.getDefault()).contains(mSearch)) {
                    Pair<ArrayList<ChosenObjectInfo>, ArrayList<Website>> entry =
                            objects.get(info.getObject());
                    if (entry == null) {
                        entry =
                                Pair.create(
                                        new ArrayList<ChosenObjectInfo>(),
                                        new ArrayList<Website>());
                        objects.put(info.getObject(), entry);
                    }
                    entry.first.add(info);
                    entry.second.add(site);
                }
            }
        }

        updateBlockedHeader(0);
        updateAllowedHeader(0, true);
        updateManagedHeader(0);

        for (Pair<ArrayList<ChosenObjectInfo>, ArrayList<Website>> entry : objects.values()) {
            Preference preference = new Preference(getStyledContext());
            Bundle extras = preference.getExtras();
            extras.putInt(ChosenObjectSettings.EXTRA_CATEGORY, mCategory.getContentSettingsType());
            extras.putString(EXTRA_TITLE, getActivity().getTitle().toString());
            extras.putSerializable(ChosenObjectSettings.EXTRA_OBJECT_INFOS, entry.first);
            extras.putSerializable(ChosenObjectSettings.EXTRA_SITES, entry.second);
            preference.setIcon(
                    SettingsUtils.getTintedIcon(
                            getContext(),
                            ContentSettingsResources.getIcon(mCategory.getContentSettingsType())));
            preference.setTitle(entry.first.get(0).getName());
            preference.setFragment(ChosenObjectSettings.class.getCanonicalName());
            getPreferenceScreen().addPreference(preference);
        }

        return objects.size() != 0;
    }

    private boolean isBlocked() {
        switch (mGlobalToggleLayout) {
            case GlobalToggleLayout.TRI_STATE_TOGGLE:
                TriStateSiteSettingsPreference triStateToggle =
                        getPreferenceScreen().findPreference(TRI_STATE_TOGGLE_KEY);
                return (triStateToggle.getCheckedSetting() == ContentSettingValues.BLOCK);
            case GlobalToggleLayout.TRI_STATE_COOKIE_TOGGLE:
                TriStateCookieSettingsPreference triStateCookieToggle =
                        getPreferenceScreen().findPreference(TRI_STATE_COOKIE_TOGGLE);
                return triStateCookieToggle.getState() != CookieControlsMode.OFF;
            case GlobalToggleLayout.BINARY_TOGGLE:
                ChromeSwitchPreference binaryToggle =
                        getPreferenceScreen().findPreference(BINARY_TOGGLE_KEY);
                if (binaryToggle != null) {
                    return !binaryToggle.isChecked();
                }
                break;
        }
        return false;
    }

    private void configureGlobalToggles() {
        int contentType = mCategory.getContentSettingsType();
        PreferenceScreen screen = getPreferenceScreen();

        // Find all preferences on the current preference screen. Some preferences are
        // not needed for the current category and will be removed in the steps below.
        ChromeSwitchPreference binaryToggle = screen.findPreference(BINARY_TOGGLE_KEY);
        TriStateSiteSettingsPreference triStateToggle = screen.findPreference(TRI_STATE_TOGGLE_KEY);
        TriStateCookieSettingsPreference triStateCookieToggle =
                screen.findPreference(TRI_STATE_COOKIE_TOGGLE);
        Preference notificationsVibrate = screen.findPreference(NOTIFICATIONS_VIBRATE_TOGGLE_KEY);
        mNotificationsQuietUiPref = screen.findPreference(NOTIFICATIONS_QUIET_UI_TOGGLE_KEY);
        mNotificationsTriStatePref = screen.findPreference(NOTIFICATIONS_TRI_STATE_PREF_KEY);
        mLocationTriStatePref = screen.findPreference(LOCATION_TRI_STATE_PREF_KEY);
        mDesktopSiteWindowPref = screen.findPreference(DESKTOP_SITE_WINDOW_TOGGLE_KEY);
        Preference explainProtectedMediaKey = screen.findPreference(EXPLAIN_PROTECTED_MEDIA_KEY);
        PreferenceGroup allowedGroup = screen.findPreference(ALLOWED_GROUP);
        PreferenceGroup blockedGroup = screen.findPreference(BLOCKED_GROUP);
        PreferenceGroup managedGroup = screen.findPreference(MANAGED_GROUP);
        boolean permissionBlockedByOs = mCategory.showPermissionBlockedMessage(getContext());

        if (mGlobalToggleLayout != GlobalToggleLayout.BINARY_TOGGLE) {
            screen.removePreference(binaryToggle);
        }
        if (mGlobalToggleLayout != GlobalToggleLayout.TRI_STATE_TOGGLE) {
            screen.removePreference(triStateToggle);
        }
        if (mGlobalToggleLayout != GlobalToggleLayout.TRI_STATE_COOKIE_TOGGLE) {
            screen.removePreference(triStateCookieToggle);
        }
        switch (mGlobalToggleLayout) {
            case GlobalToggleLayout.BINARY_TOGGLE:
                configureBinaryToggle(binaryToggle, contentType);
                break;
            case GlobalToggleLayout.TRI_STATE_TOGGLE:
                configureTriStateToggle(triStateToggle, contentType);
                break;
            case GlobalToggleLayout.TRI_STATE_COOKIE_TOGGLE:
                configureTriStateCookieToggle(triStateCookieToggle);
                break;
        }

        Preference infoText = screen.findPreference(INFO_TEXT_KEY);
        if (mCategory.getType() == SiteSettingsCategory.Type.SITE_DATA) {
            infoText.setSummary(R.string.website_settings_site_data_page_description);
        } else if (mCategory.getType() == SiteSettingsCategory.Type.THIRD_PARTY_COOKIES) {
            infoText.setSummary(R.string.website_settings_third_party_cookies_page_description);
        } else if (mCategory.getType() == SiteSettingsCategory.Type.STORAGE_ACCESS) {
            infoText.setSummary(getStorageAccessSummary());
        } else {
            screen.removePreference(infoText);
        }

        // Hide the anti-abuse text preferences, as needed.
        if (mCategory.getType() != SiteSettingsCategory.Type.ANTI_ABUSE) {
            Preference antiAbuseWhenOnHeader = screen.findPreference(ANTI_ABUSE_WHEN_ON_HEADER);
            Preference antiAbuseWhenOnSectionOne =
                    screen.findPreference(ANTI_ABUSE_WHEN_ON_SECTION_ONE);
            Preference antiAbuseWhenOnSectionTwo =
                    screen.findPreference(ANTI_ABUSE_WHEN_ON_SECTION_TWO);
            Preference antiAbuseWhenOnSectionThree =
                    screen.findPreference(ANTI_ABUSE_WHEN_ON_SECTION_THREE);
            Preference antiAbuseThingsToConsiderHeader =
                    screen.findPreference(ANTI_ABUSE_THINGS_TO_CONSIDER_HEADER);
            Preference antiAbuseThingsToConsiderSectionOne =
                    screen.findPreference(ANTI_ABUSE_THINGS_TO_CONSIDER_SECTION_ONE);

            screen.removePreference(antiAbuseWhenOnHeader);
            screen.removePreference(antiAbuseWhenOnSectionOne);
            screen.removePreference(antiAbuseWhenOnSectionTwo);
            screen.removePreference(antiAbuseWhenOnSectionThree);
            screen.removePreference(antiAbuseThingsToConsiderHeader);
            screen.removePreference(antiAbuseThingsToConsiderSectionOne);
        }

        // Show either the old or new settings UI for geolocation permissions.
        if (mCategory.getType() == SiteSettingsCategory.Type.DEVICE_LOCATION) {
            if (getSiteSettingsDelegate().isPermissionDedicatedCpssSettingAndroidFeatureEnabled()) {
                mLocationTriStatePref.initialize(
                        UserPrefs.get(getSiteSettingsDelegate().getBrowserContextHandle()));
                updateLocationSecondaryControls();
            } else {
                screen.removePreference(mLocationTriStatePref);
            }
        } else {
            screen.removePreference(mLocationTriStatePref);
        }

        if (permissionBlockedByOs) {
            maybeShowOsWarning(screen);
            screen.removePreference(notificationsVibrate);
            screen.removePreference(mNotificationsQuietUiPref);
            screen.removePreference(mNotificationsTriStatePref);
            screen.removePreference(mDesktopSiteWindowPref);
            screen.removePreference(explainProtectedMediaKey);
            screen.removePreference(allowedGroup);
            screen.removePreference(blockedGroup);
            screen.removePreference(managedGroup);
            // Since all preferences are hidden, there's nothing to do further and we can
            // simply return.
            return;
        }

        // Configure/hide the notifications secondary controls, as needed.
        if (mCategory.getType() == SiteSettingsCategory.Type.NOTIFICATIONS) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
                notificationsVibrate.setOnPreferenceChangeListener(this);
            } else {
                screen.removePreference(notificationsVibrate);
            }

            if (getSiteSettingsDelegate().isQuietNotificationPromptsFeatureEnabled()) {
                mNotificationsQuietUiPref.setOnPreferenceChangeListener(this);
            } else {
                screen.removePreference(mNotificationsQuietUiPref);
            }
            // Show either the old or new settings UI for notifications permissions.
            if (getSiteSettingsDelegate().isPermissionDedicatedCpssSettingAndroidFeatureEnabled()) {
                screen.removePreference(mNotificationsQuietUiPref);
                mNotificationsTriStatePref.initialize(
                        UserPrefs.get(getSiteSettingsDelegate().getBrowserContextHandle()));
            } else {
                screen.removePreference(mNotificationsTriStatePref);
            }
            updateNotificationsSecondaryControls();
        } else {
            screen.removePreference(notificationsVibrate);
            screen.removePreference(mNotificationsQuietUiPref);
            screen.removePreference(mNotificationsTriStatePref);
        }

        // Configure/hide the desktop site window setting, as needed.
        if (mCategory.getType() == SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE) {
            mDesktopSiteWindowPref.setOnPreferenceChangeListener(this);
            updateDesktopSiteWindowSetting();
        } else {
            screen.removePreference(mDesktopSiteWindowPref);
        }

        // Only show the link that explains protected content settings when needed.
        if (mCategory.getType() == SiteSettingsCategory.Type.PROTECTED_MEDIA
                && getSiteSettingsDelegate().isHelpAndFeedbackEnabled()) {
            explainProtectedMediaKey.setOnPreferenceClickListener(
                    preference -> {
                        getSiteSettingsDelegate()
                                .launchProtectedContentHelpAndFeedbackActivity(getActivity());
                        return true;
                    });

            // On small screens with no touch input, nested focusable items inside a LinearLayout in
            // ListView cause focus problems when using a keyboard (crbug.com/974413).
            // TODO(chouinard): Verify on a small screen device whether this patch is still needed
            // now that we've migrated this fragment to Support Library (mListView is a RecyclerView
            // now).
            mListView.setFocusable(false);
        } else {
            screen.removePreference(explainProtectedMediaKey);
            mListView.setFocusable(true);
        }

        // When this menu opens, make sure the Blocked list is collapsed.
        if (!mGroupByAllowBlock) {
            mBlockListExpanded = false;
            mAllowListExpanded = true;
            mManagedListExpanded = false;
        }
        mGroupByAllowBlock = true;

        allowedGroup.setOnPreferenceClickListener(this);
        blockedGroup.setOnPreferenceClickListener(this);
        managedGroup.setOnPreferenceClickListener(this);
    }

    private SpannableString getStorageAccessSummary() {
        final String storageAccessRawString =
                getContext().getString(R.string.website_settings_storage_access_page_description);
        final NoUnderlineClickableSpan clickableSpan =
                new NoUnderlineClickableSpan(
                        getContext(),
                        (widget) -> {
                            getSiteSettingsDelegate()
                                    .launchStorageAccessHelpActivity(getActivity());
                        });
        return SpanApplier.applySpans(
                storageAccessRawString,
                new SpanApplier.SpanInfo("<link>", "</link>", clickableSpan));
    }

    private void maybeShowOsWarning(PreferenceScreen screen) {
        if (isBlocked()) {
            return;
        }

        // Show the link to system settings since permission is disabled.
        ChromeBasePreference osWarning = new ChromeBasePreference(getStyledContext(), null);
        ChromeBasePreference osWarningExtra = new ChromeBasePreference(getStyledContext(), null);
        mCategory.configurePermissionIsOffPreferences(
                osWarning,
                osWarningExtra,
                getContext(),
                true,
                getSiteSettingsDelegate().getAppName());
        if (osWarning.getTitle() != null) {
            osWarning.setKey(SingleWebsiteSettings.PREF_OS_PERMISSIONS_WARNING);
            screen.addPreference(osWarning);
        }
        if (osWarningExtra.getTitle() != null) {
            osWarningExtra.setKey(SingleWebsiteSettings.PREF_OS_PERMISSIONS_WARNING_EXTRA);
            screen.addPreference(osWarningExtra);
        }
    }

    private void configureTriStateCookieToggle(
            TriStateCookieSettingsPreference triStateCookieToggle) {
        triStateCookieToggle.setOnPreferenceChangeListener(this);
        triStateCookieToggle.setCookiesDetailsRequestedListener(this);
        TriStateCookieSettingsPreference.Params params =
                new TriStateCookieSettingsPreference.Params();
        params.cookieControlsMode = getCookieControlsMode();
        params.cookieControlsModeEnforced = mCategory.isManaged();
        params.isIncognitoModeEnabled = getSiteSettingsDelegate().isIncognitoModeEnabled();
        params.isPrivacySandboxFirstPartySetsUIEnabled =
                getSiteSettingsDelegate().isPrivacySandboxFirstPartySetsUIFeatureEnabled();
        params.isRelatedWebsiteSetsDataAccessEnabled =
                getSiteSettingsDelegate().isRelatedWebsiteSetsDataAccessEnabled();
        triStateCookieToggle.setState(params);
    }

    private int getCookieControlsMode() {
        PrefService prefService =
                UserPrefs.get(getSiteSettingsDelegate().getBrowserContextHandle());
        return prefService.getInteger(COOKIE_CONTROLS_MODE);
    }

    private void configureTriStateToggle(
            TriStateSiteSettingsPreference triStateToggle, int contentType) {
        triStateToggle.setOnPreferenceChangeListener(this);
        @ContentSettingValues
        int setting =
                WebsitePreferenceBridge.getDefaultContentSetting(
                        getSiteSettingsDelegate().getBrowserContextHandle(), contentType);
        int[] descriptionIds =
                ContentSettingsResources.getTriStateSettingDescriptionIDs(contentType);
        triStateToggle.initialize(setting, descriptionIds);
    }

    private void configureBinaryToggle(ChromeSwitchPreference binaryToggle, int contentType) {
        binaryToggle.setOnPreferenceChangeListener(this);
        binaryToggle.setTitle(ContentSettingsResources.getTitle(contentType));

        // Set summary on or off.
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        if (mCategory.getType() == SiteSettingsCategory.Type.DEVICE_LOCATION
                && WebsitePreferenceBridge.isLocationAllowedByPolicy(browserContextHandle)) {
            binaryToggle.setSummaryOn(ContentSettingsResources.getGeolocationAllowedSummary());
        } else {
            binaryToggle.setSummaryOn(ContentSettingsResources.getEnabledSummary(contentType));
        }
        binaryToggle.setSummaryOff(ContentSettingsResources.getDisabledSummary(contentType));
        int summaryForAccessibility =
                ContentSettingsResources.getSummaryOverrideForScreenReader(contentType);
        if (summaryForAccessibility != 0) {
            binaryToggle.setSummaryOverrideForScreenReader(
                    getContext().getString(summaryForAccessibility));
        }

        binaryToggle.setManagedPreferenceDelegate(
                new SingleCategoryManagedPreferenceDelegate(
                        getSiteSettingsDelegate().getManagedPreferenceDelegate()));

        // Set the checked value.
        binaryToggle.setChecked(
                WebsitePreferenceBridge.isCategoryEnabled(browserContextHandle, contentType));
    }

    private void updateNotificationsSecondaryControls() {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        Boolean categoryEnabled =
                WebsitePreferenceBridge.isCategoryEnabled(
                        browserContextHandle, ContentSettingsType.NOTIFICATIONS);

        // The notifications vibrate checkbox.
        ChromeBaseCheckBoxPreference vibratePref =
                getPreferenceScreen().findPreference(NOTIFICATIONS_VIBRATE_TOGGLE_KEY);
        if (vibratePref != null) vibratePref.setEnabled(categoryEnabled);

        if (!getSiteSettingsDelegate().isQuietNotificationPromptsFeatureEnabled()) return;

        if (categoryEnabled) {
            if (getSiteSettingsDelegate().isPermissionDedicatedCpssSettingAndroidFeatureEnabled()) {
                getPreferenceScreen().addPreference(mNotificationsTriStatePref);
            } else {
                getPreferenceScreen().addPreference(mNotificationsQuietUiPref);
                PrefService prefService = UserPrefs.get(browserContextHandle);
                mNotificationsQuietUiPref.setChecked(
                        prefService.getBoolean(ENABLE_QUIET_NOTIFICATION_PERMISSION_UI));
            }
        } else {
            if (getSiteSettingsDelegate().isPermissionDedicatedCpssSettingAndroidFeatureEnabled()) {
                getPreferenceScreen().removePreference(mNotificationsTriStatePref);
            } else {
                getPreferenceScreen().removePreference(mNotificationsQuietUiPref);
            }
        }
    }

    private void updateLocationSecondaryControls() {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        Boolean categoryEnabled =
                WebsitePreferenceBridge.isCategoryEnabled(
                        browserContextHandle, ContentSettingsType.GEOLOCATION);
        if (getSiteSettingsDelegate().isPermissionDedicatedCpssSettingAndroidFeatureEnabled()) {
            if (categoryEnabled) {
                getPreferenceScreen().addPreference(mLocationTriStatePref);
            } else {
                getPreferenceScreen().removePreference(mLocationTriStatePref);
            }
        }
    }

    // TODO(crbug.com/40852484): Looking at a different class setup for SingleCategorySettings that
    // allows category specific logic to live in separate files.
    private void updateDesktopSiteWindowSetting() {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        Boolean categoryEnabled =
                WebsitePreferenceBridge.isCategoryEnabled(
                        browserContextHandle, ContentSettingsType.REQUEST_DESKTOP_SITE);

        if (categoryEnabled) {
            // When the global setting for RDS is on, window setting should be displayed.
            getPreferenceScreen().addPreference(mDesktopSiteWindowPref);
            PrefService prefService = UserPrefs.get(browserContextHandle);
            mDesktopSiteWindowPref.setChecked(
                    prefService.getBoolean(DESKTOP_SITE_WINDOW_SETTING_ENABLED));
        } else {
            // Otherwise, ensure window setting is hidden.
            getPreferenceScreen().removePreference(mDesktopSiteWindowPref);
        }
    }

    private void showManagedToast() {
        if (mCategory.isManagedByCustodian()) {
            ManagedPreferencesUtils.showManagedByParentToast(
                    getContext(),
                    new SingleCategoryManagedPreferenceDelegate(
                            getSiteSettingsDelegate().getManagedPreferenceDelegate()));
        } else {
            ManagedPreferencesUtils.showManagedByAdministratorToast(getContext());
        }
    }

    /**
     * Builds an alert dialog which can be used to change the preference value or remove
     * for the exception for the current categories ContentSettingType on a Website.
     */
    private AlertDialog buildPreferenceDialog(Website site) {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        @ContentSettingsType.EnumType int contentSettingsType = mCategory.getContentSettingsType();

        @ContentSettingValues
        Integer value = site.getContentSetting(browserContextHandle, contentSettingsType);

        AlertDialog alertDialog =
                new AlertDialog.Builder(getContext(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setTitle(
                                getContext()
                                        .getString(
                                                R.string.website_settings_edit_site_dialog_title))
                        .setPositiveButton(R.string.cancel, null)
                        .setNegativeButton(
                                R.string.remove,
                                (dialog, which) -> {
                                    site.setContentSetting(
                                            browserContextHandle,
                                            contentSettingsType,
                                            ContentSettingValues.DEFAULT);

                                    if (mCategory.getType()
                                            == SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT) {
                                        AutoDarkMetrics.recordAutoDarkSettingsChangeSource(
                                                AutoDarkSettingsChangeSource
                                                        .SITE_SETTINGS_EXCEPTION_LIST,
                                                false);
                                    }

                                    getInfoForOrigins();
                                    dialog.dismiss();
                                })
                        .create();

        // Set a custom view with description text and a radio button group that uses
        // RadioButtonWithDescriptionLayout.
        var inflater =
                (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        var contentView = (LinearLayout) inflater.inflate(R.layout.edit_site_dialog_content, null);

        TextView messageView = contentView.findViewById(R.id.message);
        messageView.setText(
                getContext()
                        .getString(
                                R.string.website_settings_edit_site_dialog_description,
                                site.getTitleForPreferenceRow()));

        RadioButtonWithDescriptionLayout radioGroup =
                contentView.findViewById(R.id.radio_button_group);
        RadioButtonWithDescription allowButton = radioGroup.findViewById(R.id.allow);
        allowButton.setPrimaryText(
                getString(
                        ContentSettingsResources.getSiteSummary(
                                ContentSettingValues.ALLOW, contentSettingsType)));

        RadioButtonWithDescription blockButton = radioGroup.findViewById(R.id.block);
        blockButton.setPrimaryText(
                getString(
                        ContentSettingsResources.getSiteSummary(
                                ContentSettingValues.BLOCK, contentSettingsType)));

        if (value == ContentSettingValues.ALLOW) {
            allowButton.setChecked(true);
        } else {
            blockButton.setChecked(true);
        }

        radioGroup.setOnCheckedChangeListener(
                (radioButtonGroup, i) -> {
                    @ContentSettingValues
                    int permission =
                            allowButton.isChecked()
                                    ? ContentSettingValues.ALLOW
                                    : ContentSettingValues.BLOCK;

                    site.setContentSetting(browserContextHandle, contentSettingsType, permission);

                    DesktopSiteMetrics.recordDesktopSiteSettingsChanged(
                            mCategory.getType(), permission, site);
                    getInfoForOrigins();
                    alertDialog.dismiss();
                });
        alertDialog.setView(contentView);
        return alertDialog;
    }

    /**
     * Always returns true unless a category uses custom logic to show/hide exceptions on the
     * category settings page.
     * @return Whether exceptions should be added for the category.
     */
    private boolean shouldAddExceptionsForCategory() {
        if (mCategory.getType() == SiteSettingsCategory.Type.ANTI_ABUSE) {
            return false;
        }
        return true;
    }

    /**
     * Performs a set of tasks when the user updates the desktop site content setting.
     * 1. Records the desktop site content setting change.
     * 2. Updates the Shared Preference USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY.
     * @param enabled Whether the desktop site is enabled after the change.
     */
    public static void recordSiteLayoutChanged(boolean enabled) {
        @SiteLayout int layout = enabled ? SiteLayout.DESKTOP : SiteLayout.MOBILE;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.RequestDesktopSite.Changed", layout, SiteLayout.NUM_ENTRIES);

        // TODO(crbug.com/40126122): Use SharedPreferencesManager if it is componentized.
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(
                        SingleCategorySettingsConstants
                                .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY,
                        enabled)
                .apply();
    }

    public ForwardingManagedPreferenceDelegate createWebsiteManagedPrefDelegate() {
        return new ForwardingManagedPreferenceDelegate(
                getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                WebsitePreference websitePref = (WebsitePreference) preference;
                ContentSettingException exception =
                        websitePref
                                .site()
                                .getContentSettingException(mCategory.getContentSettingsType());
                if (exception != null) {
                    return exception.getSource() == ProviderType.POLICY_PROVIDER;
                }
                return false;
            }

            /*
             * Click is always enabled as a toast will be shown if a managed preference is clicked.
             */
            @Override
            public boolean isPreferenceClickDisabled(Preference preference) {
                return false;
            }
        };
    }

    private void onOffboardingCardCloseClick() {
        mCardPreference.setVisible(false);
    }

    private void openUrlInCct(String url) {
        var customTabHelper = getCustomTabIntentHelper();
        assert (customTabHelper != null) : "CCT helpers must be set before opening a link";
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent =
                customTabHelper.createCustomTabActivityIntent(getContext(), customTabIntent.intent);
        intent.setPackage(getContext().getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(getContext(), intent);
    }
}
