// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.content.Intent;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.FragmentSettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

/** Preference fragment for showing the Site Settings UI. */
public abstract class BaseSiteSettingsFragment extends PreferenceFragmentCompat
        implements FragmentSettingsNavigation {
    private SiteSettingsDelegate mSiteSettingsDelegate;
    private CustomTabIntentHelper mCustomTabIntentHelper;
    private SettingsNavigation mSettingsNavigation;

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

    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using {@link
     * org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     * TODO(crbug.com/40751023): Update when LaunchIntentDispatcher is (partially-)modularized.
     */
    public interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }

    /**
     * Sets the CustomTabIntentHelper instance this Fragment should use.
     *
     * <p>This should be called by the embedding Activity.
     */
    public void setCustomTabIntentHelper(CustomTabIntentHelper customTabIntentHelper) {
        mCustomTabIntentHelper = customTabIntentHelper;
    }

    /** @return the CustomTabIntentHelper instance to use. */
    public CustomTabIntentHelper getCustomTabIntentHelper() {
        return mCustomTabIntentHelper;
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
    public SettingsNavigation getSettingsNavigation() {
        return mSettingsNavigation;
    }
}
