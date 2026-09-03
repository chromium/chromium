// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.preference.PreferenceFragmentCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.FragmentSettingsNavigation;
import org.chromium.components.browser_ui.settings.PreferenceUpdateObserver;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.widget.containment.ContainmentItemDecoration;

/** Preference fragment for showing the Site Settings UI. */
@NullMarked
public abstract class BaseSiteSettingsFragment extends PreferenceFragmentCompat
        implements FragmentSettingsNavigation, PreferenceUpdateObserver.Provider {
    private @Nullable SiteSettingsDelegate mSiteSettingsDelegate;
    private @Nullable SettingsNavigation mSettingsNavigation;
    private @Nullable PreferenceUpdateObserver mPreferenceUpdateObserver;

    /**
     * Sets the SiteSettingsDelegate instance this Fragment should use.
     *
     * <p>This should be called by the embedding Activity. It may be called more than once for
     * SettingsInTab, as a temporary dependency provider is used during the initial activity
     * restore, replaced later with the actual dependencies once they become available.
     */
    public void setSiteSettingsDelegate(SiteSettingsDelegate client) {
        mSiteSettingsDelegate = client;
    }

    /** @return the SiteSettingsDelegate instance to use when rendering the Site Settings UI. */
    public SiteSettingsDelegate getSiteSettingsDelegate() {
        assert mSiteSettingsDelegate != null : "SiteSettingsDelegate not set";
        return mSiteSettingsDelegate;
    }

    /** @return Whether a SiteSettingsDelegate instance has been assigned to this Fragment. */
    public boolean hasSiteSettingsDelegate() {
        return mSiteSettingsDelegate != null;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (mSiteSettingsDelegate != null) {
            mSiteSettingsDelegate.onDestroyView();
        }
    }

    @Override
    public void setSettingsNavigation(SettingsNavigation settingsNavigation) {
        mSettingsNavigation = settingsNavigation;
    }

    /** Returns the associated {@link SettingsNavigation}. */
    public @Nullable SettingsNavigation getSettingsNavigation() {
        return mSettingsNavigation;
    }

    @Override
    public void setPreferenceUpdateObserver(PreferenceUpdateObserver observer) {
        mPreferenceUpdateObserver = observer;
    }

    @Override
    public void removePreferenceUpdateObserver() {
        mPreferenceUpdateObserver = null;
    }

    /** Notifies the observer that the preferences have been updated. */
    protected void notifyPreferencesUpdated() {
        if (mPreferenceUpdateObserver != null) {
            mPreferenceUpdateObserver.onPreferencesUpdated(this);
        }
    }

    /**
     * Updates the containment styling for this fragment.
     *
     * <p>Delegating through {@link #notifyPreferencesUpdated()} ensures that the host container
     * (such as {@code SettingsHostFragment} or {@code SettingsActivity}) defers containment styling
     * until the next layout completion pass via {@code
     * SettingsContainmentHelper.postUpdateContainmentOnLayout()}. This guarantees that
     * asynchronously populated preferences (e.g. in {@code AllSiteSettings}) are fully measured,
     * bound, and attached in the RecyclerView hierarchy before item decoration styles and card
     * backgrounds are calculated and drawn.
     *
     * <p>If no host observer is attached (e.g. in standalone fragment tests), fallback scheduling
     * via {@link View#post(Runnable)} defers decoration invalidation to avoid styling before views
     * are laid out.
     */
    protected void updateContainment() {
        if (!getSiteSettingsDelegate().isSettingsContainmentEnabled()) {
            return;
        }

        if (mPreferenceUpdateObserver != null) {
            notifyPreferencesUpdated();
            return;
        }

        RecyclerView listView = getListView();
        if (listView == null) {
            return;
        }

        listView.post(
                () -> {
                    if (getView() == null) return;

                    ContainmentItemDecoration decoration = null;
                    for (int i = 0; i < listView.getItemDecorationCount(); i++) {
                        RecyclerView.ItemDecoration item = listView.getItemDecorationAt(i);
                        if (item instanceof ContainmentItemDecoration containmentItemDecoration) {
                            decoration = containmentItemDecoration;
                            break;
                        }
                    }

                    if (decoration != null) {
                        decoration.updatePreferenceStyles(
                                decoration
                                        .getStylingController()
                                        .generatePreferenceStyles(
                                                SettingsUtils.getVisiblePreferences(
                                                        getPreferenceScreen())));
                        listView.invalidateItemDecorations();
                    }
                });
    }
}
