// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.settings.SearchUtils.handleSearchNavigation;

import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.format.Formatter;
import android.text.style.ForegroundColorSpan;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;
import androidx.preference.PreferenceManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SearchUtils;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/**
 * Shows a list of all sites. When the user selects a site, SingleWebsiteSettings is launched to
 * allow the user to see or modify the settings for that particular website.
 */
@UsedByReflection("all_site_preferences.xml")
public class AllSiteSettings extends BaseSiteSettingsFragment
        implements EmbeddableSettingsPage,
                PreferenceManager.OnPreferenceTreeClickListener,
                View.OnClickListener,
                CustomDividerFragment {
    // The key to use to pass which category this preference should display,
    // should only be All Sites or Storage.
    public static final String EXTRA_CATEGORY = "category";
    public static final String EXTRA_TITLE = "title";
    public static final String EXTRA_SEARCH = "search";

    /**
     * If present, the list of websites will be filtered by domain using {@link
     * UrlUtilities#getDomainAndRegistry}.
     */
    public static final String EXTRA_SELECTED_DOMAINS = "selected_domains";

    public static final String PREF_CLEAR_BROWSING_DATA = "clear_browsing_data_link";
    // Prefix for Related Website Sets search filter (mobile-desktop feature parity)
    public static final String RWS_SEARCH_PREFIX = "related:";

    // The clear button displayed in the Storage view.
    private Button mClearButton;
    // The list that contains preferences.
    private RecyclerView mListView;
    // The view to show when the list is empty.
    private TextView mEmptyView;
    // The item for searching the list of items.
    private MenuItem mSearchItem;
    // The Site Settings Category we are showing.
    private SiteSettingsCategory mCategory;
    // If not blank, represents a substring to use to search for site names.
    private String mSearch;
    // The websites that are currently displayed to the user.
    private List<WebsitePreference> mWebsites;
    private PropertyModel mDialogModel;
    private ModalDialogManager mDialogManager;

    @Nullable private Set<String> mSelectedDomains;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    private class ResultsPopulator implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            // This method may be called after the activity has been destroyed.
            // In that case, bail out.
            if (getActivity() == null) return;
            mWebsites = null;

            resetList();

            boolean hasEntries = addWebsites(sites);

            if (mEmptyView == null) return;

            mEmptyView.setVisibility(hasEntries ? View.GONE : View.VISIBLE);
        }
    }

    private void getInfoForOrigins() {
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(getSiteSettingsDelegate(), false);
        fetcher.fetchPreferencesForCategoryAndPopulateRwsInfo(mCategory, new ResultsPopulator());
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
        if (mCategory == null) {
            mCategory =
                    SiteSettingsCategory.createFromType(
                            browserContextHandle, SiteSettingsCategory.Type.ALL_SITES);
        }
        if (mCategory.getType() == SiteSettingsCategory.Type.ZOOM) {
            mCategory =
                    SiteSettingsCategory.createFromType(
                            browserContextHandle, SiteSettingsCategory.Type.ZOOM);
        }
        if (!(mCategory.getType() == SiteSettingsCategory.Type.ALL_SITES
                || mCategory.getType() == SiteSettingsCategory.Type.USE_STORAGE
                || mCategory.getType() == SiteSettingsCategory.Type.ZOOM)) {
            throw new IllegalArgumentException("Use SingleCategorySettings instead.");
        }

        ViewGroup view = (ViewGroup) super.onCreateView(inflater, container, savedInstanceState);

        // Add custom views for Storage Preferences to bottom of the fragment.
        if (mCategory.getType() == SiteSettingsCategory.Type.USE_STORAGE) {
            inflater.inflate(R.layout.storage_preferences_view, view, true);
            mEmptyView = view.findViewById(R.id.empty_storage);
            mClearButton = view.findViewById(R.id.clear_button);
            mClearButton.setOnClickListener(this);
        }

        // Add custom views for Zoom Preferences to bottom of the fragment.
        if (mCategory.getType() == SiteSettingsCategory.Type.ZOOM) {
            inflater.inflate(R.layout.zoom_preferences_view, view, true);
            mEmptyView = view.findViewById(R.id.site_settings_zoom_empty_zoom_levels_message_text);
            mClearButton = view.findViewById(R.id.site_settings_zoom_clear_all_zoom_levels_button);
            mClearButton.setOnClickListener(this::handleZoomClearAll);
        }

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

    /**
     * This clears all the storage for websites that are displayed to the user. This happens
     * asynchronously, and then we call {@link #getInfoForOrigins()} when we're done.
     */
    public void clearStorage() {
        if (mWebsites == null) return;
        RecordUserAction.record("MobileSettingsStorageClearAll");

        // The goal is to refresh the info for origins again after we've cleared all of them, so we
        // wait until the last website is cleared to refresh the origin list.
        final int[] numLeft = new int[1];
        numLeft[0] = mWebsites.size();
        for (int i = 0; i < mWebsites.size(); i++) {
            WebsitePreference preference = mWebsites.get(i);
            preference
                    .site()
                    .clearAllStoredData(
                            getSiteSettingsDelegate(),
                            () -> {
                                if (--numLeft[0] <= 0) getInfoForOrigins();
                            });
        }
    }

    /** This resets the zooms for all websites to the default zoom set in Chrome Site Settings. */
    public void clearZooms() {
        BrowserContextHandle browserContextHandle =
                getSiteSettingsDelegate().getBrowserContextHandle();
        double defaultZoomFactor =
                PageZoomUtils.getDefaultZoomLevelAsZoomFactor(browserContextHandle);
        for (WebsitePreference preference : mWebsites) {
            // Propagate the change through HostZoomMap.
            HostZoomMap.setZoomLevelForHost(
                    browserContextHandle,
                    preference.site().getAddress().getHost(),
                    defaultZoomFactor);
        }
        // Refresh this fragment to trigger UI change.
        getInfoForOrigins();
    }

    /** OnClickListener for the zoom button **/
    public void handleZoomClearAll(View v) {
        Resources resources = getContext().getResources();
        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, makeController())
                        .with(
                                ModalDialogProperties.TITLE,
                                resources,
                                R.string.zoom_clear_all_zooms_dialog_title)
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                getContext()
                                        .getString(
                                                R.string
                                                        .site_settings_clear_all_zoom_levels_warning))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.delete)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .build();

        mDialogManager =
                new ModalDialogManager(new AppModalPresenter(getContext()), ModalDialogType.APP);
        mDialogManager.showDialog(mDialogModel, ModalDialogType.APP);
    }

    private ModalDialogProperties.Controller makeController() {
        return new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                switch (buttonType) {
                    case ButtonType.POSITIVE:
                        clearZooms();
                        mDialogManager.destroy();
                        break;
                    case ButtonType.NEGATIVE:
                        mDialogManager.destroy();
                        break;
                    default:
                        // Should never reach this case, as there are only two buttons displayed.
                        assert false;
                        break;
                }
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        };
    }

    /** OnClickListener for the clear button. We show an alert dialog to confirm the action */
    @Override
    public void onClick(View v) {
        if (getContext() == null || v != mClearButton) return;

        long totalUsage = 0;
        boolean includesApps = false;
        Set<String> originsWithInstalledApp =
                getSiteSettingsDelegate().getOriginsWithInstalledApp();
        if (mWebsites != null) {
            for (WebsitePreference preference : mWebsites) {
                totalUsage += preference.site().getTotalUsage();
                if (!includesApps) {
                    includesApps =
                            originsWithInstalledApp.contains(
                                    preference.site().getAddress().getOrigin());
                }
            }
        }

        AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
        LayoutInflater inflater =
                (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View dialogView = inflater.inflate(R.layout.clear_data_dialog, null);
        TextView message = dialogView.findViewById(android.R.id.message);
        TextView signedOutText = dialogView.findViewById(R.id.signed_out_text);
        TextView offlineText = dialogView.findViewById(R.id.offline_text);
        signedOutText.setText(R.string.webstorage_clear_data_dialog_sign_out_all_message);
        offlineText.setText(R.string.webstorage_delete_data_dialog_offline_message);
        String dialogFormattedText =
                getString(
                        includesApps
                                ? R.string.webstorage_delete_data_dialog_message_with_app
                                : R.string.webstorage_delete_data_dialog_message,
                        Formatter.formatShortFileSize(getContext(), totalUsage));
        message.setText(dialogFormattedText);
        builder.setView(dialogView);
        builder.setPositiveButton(
                R.string.storage_delete_dialog_clear_storage_option,
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        clearStorage();
                    }
                });
        builder.setNegativeButton(R.string.cancel, null);
        builder.setTitle(R.string.storage_delete_site_storage_title);
        builder.create().show();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        // Handled in onActivityCreated. Moving the addPreferencesFromResource call up to here
        // causes animation jank (crbug.com/985734).
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        addPreferencesFromXml();

        String title = getArguments().getString(EXTRA_TITLE);
        if (title != null) mPageTitle.set(title);

        mSelectedDomains =
                getArguments().containsKey(EXTRA_SELECTED_DOMAINS)
                        ? new HashSet<>(getArguments().getStringArrayList(EXTRA_SELECTED_DOMAINS))
                        : null;

        mSearch =
                getArguments().containsKey(EXTRA_SEARCH)
                        ? RWS_SEARCH_PREFIX + getArguments().getString(EXTRA_SEARCH)
                        : mSearch;

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
            getSiteSettingsDelegate().launchSettingsHelpAndFeedbackActivity(getActivity());

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
        // Store in a local variable; otherwise the linter complains.
        if (preference instanceof WebsitePreference) {
            WebsitePreference website = (WebsitePreference) preference;
            website.setFragment(SingleWebsiteSettings.class.getName());
            // EXTRA_SITE re-uses already-fetched permissions, which we can only use if the Website
            // was populated with data for all permission types.
            website.putSiteIntoExtras(SingleWebsiteSettings.EXTRA_SITE);
        } else if (preference instanceof WebsiteRowPreference) {
            ((WebsiteRowPreference) preference)
                    .handleClick(getArguments(), /* fromGrouped= */ false);
        }

        return super.onPreferenceTreeClick(preference);
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

    /** Reset the preference screen and initialize it again. */
    private void resetList() {
        // This will remove the combo box at the top and all the sites listed below it.
        getPreferenceScreen().removeAll();
        // And this will add the filter preference back (combo box).
        addPreferencesFromXml();
    }

    private void addPreferencesFromXml() {
        if (mCategory.getType() == SiteSettingsCategory.Type.ALL_SITES) {
            SettingsUtils.addPreferencesFromResource(this, R.xml.all_site_preferences_v2);
            ChromeBasePreference clearBrowsingDataLink = findPreference(PREF_CLEAR_BROWSING_DATA);
            if (!getSiteSettingsDelegate().canLaunchClearBrowsingDataDialog()) {
                getPreferenceScreen().removePreference(clearBrowsingDataLink);
                return;
            }
            SpannableString spannableString =
                    new SpannableString(
                            getResources().getString(R.string.clear_browsing_data_link));
            spannableString.setSpan(
                    new ForegroundColorSpan(
                            getContext().getColor(R.color.default_text_color_link_baseline)),
                    0,
                    spannableString.length(),
                    Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
            clearBrowsingDataLink.setSummary(spannableString);
            clearBrowsingDataLink.setOnPreferenceClickListener(
                    pref -> {
                        getSiteSettingsDelegate().launchClearBrowsingDataDialog(getActivity());
                        return true;
                    });
        } else {
            SettingsUtils.addPreferencesFromResource(this, R.xml.all_site_preferences);
        }
    }

    private boolean addWebsites(Collection<Website> sites) {
        filterSelectedDomains(sites);
        if (mCategory.getType() == SiteSettingsCategory.Type.ALL_SITES) {
            List<WebsiteEntry> entries = WebsiteGroup.groupWebsites(sites);
            List<WebsiteRowPreference> preferences = new ArrayList<>();
            // Find entries matching the current search.
            for (WebsiteEntry entry : entries) {
                if (filterSearchResult(entry)) {
                    WebsiteRowPreference preference =
                            new WebsiteRowPreference(
                                    getStyledContext(),
                                    getSiteSettingsDelegate(),
                                    entry,
                                    getActivity().getLayoutInflater());
                    preference.setOnDeleteCallback(
                            () -> {
                                getInfoForOrigins();
                            });
                    preferences.add(preference);
                }
            }
            Collections.sort(preferences);
            for (WebsiteRowPreference preference : preferences) {
                getPreferenceScreen().addPreference(preference);
            }
            return !preferences.isEmpty();
        } else {
            List<WebsitePreference> websites = new ArrayList<>();
            // Find origins matching the current search.
            for (Website site : sites) {
                if (mSearch == null || mSearch.isEmpty() || site.getTitle().contains(mSearch)) {
                    WebsitePreference preference =
                            new WebsitePreference(
                                    getStyledContext(), getSiteSettingsDelegate(), site, mCategory);
                    preference.setRefreshZoomsListFunction(this::getInfoForOrigins);
                    websites.add(preference);
                }
            }
            Collections.sort(websites);
            for (WebsitePreference website : websites) {
                getPreferenceScreen().addPreference(website);
            }
            mWebsites = websites;
            return !websites.isEmpty();
        }
    }

    private boolean filterSearchResult(WebsiteEntry entry) {
        if (getSiteSettingsDelegate().shouldShowPrivacySandboxRwsUi()
                && mSearch != null
                && mSearch.startsWith(RWS_SEARCH_PREFIX)) {
            return entry.isPartOfRws()
                    && entry.getRwsOwner()
                            .contains(
                                    mSearch.replace(
                                            RWS_SEARCH_PREFIX,
                                            "")); // no need to check empty and null
        } else {
            return mSearch == null || mSearch.isEmpty() || entry.matches(mSearch);
        }
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
}
