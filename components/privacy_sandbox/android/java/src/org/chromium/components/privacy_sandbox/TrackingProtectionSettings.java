// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.browser_ui.settings.SearchUtils.handleSearchNavigation;
import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import android.os.Bundle;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.Preference.OnPreferenceClickListener;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SearchUtils;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.site_settings.AddExceptionPreference;
import org.chromium.components.browser_ui.site_settings.AddExceptionPreference.SiteAddedCallback;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Locale;

/** Fragment to manage settings for tracking protection. */
@NullMarked
public class TrackingProtectionSettings extends PrivacySandboxBaseFragment
        implements CustomDividerFragment, OnPreferenceClickListener, SiteAddedCallback {
    private static final String PREF_BLOCK_ALL_TOGGLE = "block_all_3pcd_toggle";
    private static final String PREF_BULLET_TWO = "bullet_point_two";
    private static final String ALLOWED_GROUP = "allowed_group";
    public static final String ADD_EXCEPTION_KEY = "add_exception";
    @VisibleForTesting static final String PREF_DNT_TOGGLE = "dnt_toggle";

    public static final String LEARN_MORE_URL =
            "https://support.google.com/chrome/?p=pause_protections";

    // The number of sites that are on the Allowed list.
    private int mAllowedSiteCount;

    // Whether the Allowed list should be shown expanded.
    private boolean mAllowListExpanded = true;

    // The item for searching the list of items.
    private @Nullable MenuItem mSearchItem;

    // If not blank, represents a substring to use to search for site names.
    private @Nullable String mSearch;

    private TrackingProtectionDelegate mDelegate;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.tracking_protection_preferences);
        mPageTitle.set(getString(R.string.third_party_cookies_page_title));

        setHasOptionsMenu(true);

        // Format the Learn More link in the second bullet point.
        TextMessagePreference bulletTwo = findPreference(PREF_BULLET_TWO);
        bulletTwo.setSummary(
                SpanApplier.applySpans(
                        getResources()
                                .getString(
                                        R.string
                                                .privacy_sandbox_tracking_protection_bullet_two_description),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        getContext(), (view) -> onLearnMoreClicked()))));

        ChromeSwitchPreference blockAll3pCookiesSwitch = findPreference(PREF_BLOCK_ALL_TOGGLE);

        // Block all 3rd party cookies switch.
        blockAll3pCookiesSwitch.setChecked(mDelegate.isBlockAll3pcEnabled());
        blockAll3pCookiesSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    mDelegate.setBlockAll3pc((boolean) newValue);
                    return true;
                });
        mAllowListExpanded = true;
        mAllowedSiteCount = 0;
        ExpandablePreferenceGroup allowedGroup =
                getPreferenceScreen().findPreference(ALLOWED_GROUP);
        allowedGroup.setOnPreferenceClickListener(this);
        refreshBlockingExceptions();

        // Add the exceptions button.
        SiteSettingsCategory cookiesCategory =
                SiteSettingsCategory.createFromType(
                        mDelegate.getBrowserContext(),
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        getPreferenceScreen()
                .addPreference(
                        new AddExceptionPreference(
                                getContext(),
                                ADD_EXCEPTION_KEY,
                                getString(
                                        R.string
                                                .website_settings_third_party_cookies_page_add_allow_exception_description),
                                /* isEnabled= */ true,
                                cookiesCategory,
                                this));
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        inflater.inflate(R.menu.tracking_protection_menu, menu);

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
                    if (queryHasChanged) refreshBlockingExceptions();
                });

        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_site_settings_help, Menu.NONE, R.string.menu_help);
        help.setIcon(
                TraceEventVectorDrawableCompat.create(
                        getResources(), R.drawable.ic_help_and_feedback, getContext().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        assumeNonNull(mSearchItem);

        if (item.getItemId() == R.id.menu_id_site_settings_help) {
            mDelegate
                    .getSiteSettingsDelegate(getContext())
                    .launchSettingsHelpAndFeedbackActivity(getActivity());
            return true;
        }
        if (handleSearchNavigation(item, mSearchItem, mSearch, getActivity())) {
            boolean queryHasChanged = mSearch != null && !mSearch.isEmpty();
            mSearch = null;
            if (queryHasChanged) refreshBlockingExceptions();
            return true;
        }
        return false;
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mSearch == null && mSearchItem != null) {
            SearchUtils.clearSearch(mSearchItem, getActivity());
            mSearch = null;
        }
        refreshBlockingExceptions();
    }

    @Override
    public boolean hasDivider() {
        // Remove dividers between preferences.
        return false;
    }

    // OnPreferenceClickListener:
    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (ALLOWED_GROUP.equals(preference.getKey())) {
            mAllowListExpanded = !mAllowListExpanded;
        }
        refreshBlockingExceptions();
        return true;
    }

    // AddExceptionPreference.SiteAddedCallback:
    @Override
    public void onAddSite(String primaryPattern, String secondaryPattern) {
        WebsitePreferenceBridge.setContentSettingCustomScope(
                mDelegate.getBrowserContext(),
                ContentSettingsType.COOKIES,
                primaryPattern,
                secondaryPattern,
                ContentSetting.ALLOW);

        String hostname = primaryPattern.equals(SITE_WILDCARD) ? secondaryPattern : primaryPattern;
        Toast.makeText(
                        getContext(),
                        getContext().getString(R.string.website_settings_add_site_toast, hostname),
                        Toast.LENGTH_SHORT)
                .show();

        refreshBlockingExceptions();
    }

    @Initializer
    public void setTrackingProtectionDelegate(TrackingProtectionDelegate delegate) {
        mDelegate = delegate;
    }

    private void refreshBlockingExceptions() {
        SiteSettingsCategory cookiesCategory =
                SiteSettingsCategory.createFromType(
                        mDelegate.getBrowserContext(),
                        SiteSettingsCategory.Type.THIRD_PARTY_COOKIES);
        new WebsitePermissionsFetcher(mDelegate.getSiteSettingsDelegate(getContext()))
                .fetchPreferencesForCategory(cookiesCategory, this::onExceptionsFetched);
    }

    private void onExceptionsFetched(Collection<Website> sites) {
        List<WebsiteExceptionRowPreference> websites = new ArrayList<>();
        for (Website site : sites) {
            // Filter out any exceptions for first-party cookies.
            if (site.representsThirdPartiesOnSite()
                    && (mSearch == null
                            || mSearch.isEmpty()
                            || site.getTitle().contains(mSearch))) {
                WebsiteExceptionRowPreference preference =
                        new WebsiteExceptionRowPreference(
                                getContext(), site, mDelegate, this::refreshBlockingExceptions);
                websites.add(preference);
            }
        }

        ExpandablePreferenceGroup allowedGroup =
                getPreferenceScreen().findPreference(ALLOWED_GROUP);
        allowedGroup.removeAll();
        // Add the description preference.
        var description = new TextMessagePreference(getContext(), null);
        description.setSummary(getString(R.string.tracking_protection_allowed_group_description));
        allowedGroup.addPreference(description);
        mAllowedSiteCount = 0;
        for (WebsiteExceptionRowPreference website : websites) {
            allowedGroup.addPreference(website);
            mAllowedSiteCount++;
        }
        if (!mAllowListExpanded) allowedGroup.removeAll();
        updateExceptionsHeader();
    }

    private void updateExceptionsHeader() {
        ExpandablePreferenceGroup allowedGroup =
                getPreferenceScreen().findPreference(ALLOWED_GROUP);
        SpannableStringBuilder spannable =
                new SpannableStringBuilder(
                        getString(R.string.tracking_protection_allowed_group_title));
        String prefCount = String.format(Locale.getDefault(), " - %d", mAllowedSiteCount);
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

        // Configure the preference group.
        allowedGroup.setTitle(spannable);
        allowedGroup.setExpanded(mAllowListExpanded);
    }

    private void onLearnMoreClicked() {
        getCustomTabLauncher().openUrlInCct(getContext(), LEARN_MORE_URL);
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
