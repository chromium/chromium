// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.preference.PreferenceFragmentCompat;

/**
 * Preference fragment for showing the Site Settings UI.
 */
public abstract class SiteSettingsPreferenceFragment extends PreferenceFragmentCompat {
    private SiteSettingsDelegate mSiteSettingsDelegate;

    /**
     * Sets the SiteSettingsDelegate instance this Fragment should use.
     *
     * This should be called by the embedding Activity.
     */
    public void setSiteSettingsDelegate(SiteSettingsDelegate client) {
        assert mSiteSettingsDelegate == null;
        mSiteSettingsDelegate = client;
    }

    /**
     * @return the SiteSettingsDelegate instance to use when rendering the Site Settings UI.
     */
    public SiteSettingsDelegate getSiteSettingsDelegate() {
        assert mSiteSettingsDelegate != null : "SiteSettingsDelegate not set";
        return mSiteSettingsDelegate;
    }

    /**
     * @return Whether a SiteSettingsDelegate instance has been assigned to this Fragment.
     */
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
}
