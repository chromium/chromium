// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.widget.SearchView;
import androidx.core.view.MenuItemCompat;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.BuildConfig;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Locale;

/**
 * Shows a particular chosen object (e.g. a USB device) and the list of sites that have been granted
 * access to it by the user.
 */
public class ChosenObjectSettings extends BaseSiteSettingsFragment
        implements EmbeddableSettingsPage, CustomDividerFragment {
    public static final String EXTRA_OBJECT_INFOS = "org.chromium.chrome.preferences.object_infos";
    public static final String EXTRA_SITES = "org.chromium.chrome.preferences.site_set";
    public static final String EXTRA_CATEGORY =
            "org.chromium.chrome.preferences.content_settings_type";

    // The site settings category we are showing.
    private SiteSettingsCategory mCategory;
    // The set of object permissions being examined.
    private ArrayList<ChosenObjectInfo> mObjectInfos;
    // The set of sites to display.
    private ArrayList<Website> mSites;
    // The view for searching the list of items.
    private SearchView mSearchView;
    // If not blank, represents a substring to use to search for site names.
    private String mSearch = "";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        // Set empty preferences screen.
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        setPreferenceScreen(screen);
    }

    @Override
    @SuppressWarnings("unchecked")
    public void onActivityCreated(Bundle savedInstanceState) {
        int contentSettingsType = getArguments().getInt(EXTRA_CATEGORY);
        mCategory =
                SiteSettingsCategory.createFromContentSettingsType(
                        getSiteSettingsDelegate().getBrowserContextHandle(), contentSettingsType);
        mObjectInfos =
                (ArrayList<ChosenObjectInfo>) getArguments().getSerializable(EXTRA_OBJECT_INFOS);
        checkObjectConsistency();
        mSites = (ArrayList<Website>) getArguments().getSerializable(EXTRA_SITES);
        String title = getArguments().getString(SingleCategorySettings.EXTRA_TITLE);
        if (title != null) mPageTitle.set(title);

        setHasOptionsMenu(true);

        super.onActivityCreated(savedInstanceState);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public boolean hasDivider() {
        return false;
    }

    /**
     * Checks the consistency of |mObjectInfos|.
     *
     * This method asserts that for all the entries in |mObjectInfos| the getObject() method
     * returns the same value. This must be true because this activity is displaying permissions
     * for a single object. Each instance varies only in which site it represents.
     */
    private void checkObjectConsistency() {
        if (BuildConfig.ENABLE_ASSERTS) {
            String exampleObject = mObjectInfos.get(0).getObject();
            for (ChosenObjectInfo info : mObjectInfos) {
                assert info.getObject().equals(exampleObject);
            }
        }
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        inflater.inflate(R.menu.website_preferences_menu, menu);

        MenuItem searchItem = menu.findItem(R.id.search);
        mSearchView = (SearchView) MenuItemCompat.getActionView(searchItem);
        mSearchView.setImeOptions(EditorInfo.IME_FLAG_NO_FULLSCREEN);
        SearchView.OnQueryTextListener queryTextListener =
                new SearchView.OnQueryTextListener() {
                    @Override
                    public boolean onQueryTextSubmit(String query) {
                        return true;
                    }

                    @Override
                    public boolean onQueryTextChange(String query) {
                        // Make search case-insensitive.
                        query = query.toLowerCase(Locale.getDefault());

                        if (query.equals(mSearch)) return true;

                        mSearch = query;
                        getInfo();
                        return true;
                    }
                };
        mSearchView.setOnQueryTextListener(queryTextListener);

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
        return false;
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mSites == null) {
            getInfo();
        } else {
            resetList();
        }
    }

    /**
     * Iterates through |mObjectInfos| to revoke the object permissions. If the list of objects
     * contains one that is managed, a toast will display to explain that the managed objects cannot
     * be reset, otherwise all of the objects are reset so the activity is finished.
     */
    public void revokeObjectPermissions() {
        boolean hasManagedObject = false;
        for (ChosenObjectInfo info : mObjectInfos) {
            if (info.isManaged()) {
                hasManagedObject = true;
            } else {
                info.revoke(getSiteSettingsDelegate().getBrowserContextHandle());
            }
        }

        // Managed objects cannot be revoked, so finish the activity only if the list did not
        // contain managed objects.
        if (hasManagedObject) {
            ManagedPreferencesUtils.showManagedSettingsCannotBeResetToast(getContext());
        } else {
            getSettingsNavigation().finishCurrentSettings(this);
        }
    }

    private class ResultsPopulator implements WebsitePermissionsFetcher.WebsitePermissionsCallback {
        @Override
        public void onWebsitePermissionsAvailable(Collection<Website> sites) {
            // This method may be called after the activity has been destroyed.
            // In that case, bail out.
            if (getActivity() == null) return;

            // Remember the object being examined in this view so that we can filter the results
            // to only include sites with permission for this particular object.
            String exampleObject = mObjectInfos.get(0).getObject();

            mObjectInfos.clear();
            mSites = new ArrayList<Website>();
            for (Website site : sites) {
                for (ChosenObjectInfo info : site.getChosenObjectInfo()) {
                    if (info.getObject().equals(exampleObject)) {
                        mObjectInfos.add(info);
                        if (mSearch.isEmpty()
                                || site.getTitle()
                                        .toLowerCase(Locale.getDefault())
                                        .contains(mSearch)) {
                            mSites.add(site);
                        }
                    }
                }
            }

            // After revoking a site's permission to access an object the user may end up back at
            // this activity. It is awkward to display this empty list because there's no action
            // that can be taken from it. In this case we dismiss this activity as well, taking
            // them back to SingleCategorySettings which will now no longer offer the option to
            // examine the permissions for this object.
            if (mObjectInfos.isEmpty()) {
                getSettingsNavigation().finishCurrentSettings(ChosenObjectSettings.this);
            } else {
                resetList();
            }
        }
    }

    /**
     * Refreshes the list of sites with access to the object being examined.
     *
     * <p>resetList() is called to refresh the view when the data is ready.
     */
    private void getInfo() {
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(getSiteSettingsDelegate());
        fetcher.fetchPreferencesForCategory(mCategory, new ResultsPopulator());
    }

    /**
     * Configures two Preferences that make up the header portion of this fragment. The first
     * Preference displays the chosen object name and provides a button to reset all of the site
     * permissions for the object. The second Preference is a horizontal divider line.
     */
    private void createHeader() {
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        ChromeImageViewPreference header = new ChromeImageViewPreference(getStyledContext());
        String titleText = mObjectInfos.get(0).getName();
        String dialogMsg =
                getView()
                        .getContext()
                        .getString(
                                R.string.chosen_object_website_reset_confirmation_for, titleText);

        header.setTitle(titleText);
        header.setImageView(
                R.drawable.ic_delete_white_24dp,
                R.string.website_settings_revoke_all_permissions_for_device,
                (View view) -> {
                    new AlertDialog.Builder(
                                    getContext(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                            .setTitle(R.string.reset)
                            .setMessage(dialogMsg)
                            .setPositiveButton(
                                    R.string.reset,
                                    (DialogInterface dialog, int which) -> {
                                        revokeObjectPermissions();
                                        getInfo();
                                    })
                            .setNegativeButton(R.string.cancel, null)
                            .show();
                });
        preferenceScreen.addPreference(header);

        // TODO(chouinard): Handle this header and divider in a cleaner way. May need to migrate
        // WebsitePreference to extend ChromeBasePreference to more easily set dividers
        // programmatically.
        Preference divider = new Preference(getStyledContext());
        divider.setLayoutResource(R.layout.horizontal_divider);
        preferenceScreen.addPreference(divider);
    }

    /**
     * Refreshes the preference list by recreating the heading and the sites allowed to access the
     * chosen object preference for this fragment.
     */
    private void resetList() {
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        preferenceScreen.removeAll();
        createHeader();

        // Each item |i| in |mSites| and |mObjectInfos| correspond to each other.
        // See SingleCategorySettings.addChosenObjects().
        for (int i = 0; i < mSites.size() && i < mObjectInfos.size(); ++i) {
            Website site = mSites.get(i);
            ChosenObjectInfo info = mObjectInfos.get(i);
            WebsitePreference preference =
                    new WebsitePreference(
                            getStyledContext(), getSiteSettingsDelegate(), site, mCategory);

            preference.getExtras().putSerializable(SingleWebsiteSettings.EXTRA_SITE, site);
            preference.setFragment(SingleWebsiteSettings.class.getCanonicalName());
            preference.setImageView(
                    R.drawable.ic_delete_white_24dp,
                    R.string.website_settings_revoke_device_permission,
                    (View view) -> {
                        info.revoke(getSiteSettingsDelegate().getBrowserContextHandle());
                        getInfo();
                    });

            preference.setManagedPreferenceDelegate(
                    new ForwardingManagedPreferenceDelegate(
                            getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return info.isManaged();
                        }

                        @Override
                        public boolean isPreferenceClickDisabled(Preference preference) {
                            return false;
                        }
                    });

            preferenceScreen.addPreference(preference);
        }

        // Force this list to be reloaded if the activity is resumed.
        mSites = null;
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }
}
