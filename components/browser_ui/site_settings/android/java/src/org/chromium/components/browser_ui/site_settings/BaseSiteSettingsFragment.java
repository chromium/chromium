// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.FragmentSettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

/** Preference fragment for showing the Site Settings UI. */
@NullMarked
public abstract class BaseSiteSettingsFragment extends PreferenceFragmentCompat
        implements FragmentSettingsNavigation {
    private @Nullable SiteSettingsDelegate mSiteSettingsDelegate;
    private @Nullable SettingsNavigation mSettingsNavigation;

    /**
     * Sets the SiteSettingsDelegate instance this Fragment should use.
     *
     * <p>This should be called by the embedding Activity.
     */
    public void setSiteSettingsDelegate(SiteSettingsDelegate client) {
        assert mSiteSettingsDelegate == null;
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
}
