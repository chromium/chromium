// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;

import androidx.fragment.app.FragmentManager;

import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;

/** Abstract class for controllers that use a BaseSiteSettingsFragment as subpage. */
public abstract class PageInfoPreferenceSubpageController implements PageInfoSubpageController {
    private final PageInfoControllerDelegate mDelegate;
    private BaseSiteSettingsFragment mSubPage;

    public PageInfoPreferenceSubpageController(PageInfoControllerDelegate delegate) {
        mDelegate = delegate;
    }

    protected PageInfoControllerDelegate getDelegate() {
        return mDelegate;
    }

    /**
     * @param fragment The fragment that should be added.
     * @return The view for the fragment or null if the fragment couldn't get added.
     */
    protected View addSubpageFragment(BaseSiteSettingsFragment fragment) {
        assert mSubPage == null;

        FragmentManager fragmentManager = mDelegate.getFragmentManager();
        // If the activity is getting destroyed or saved, it is not allowed to modify fragments.
        if (fragmentManager.isStateSaved()) return null;

        mSubPage = fragment;
        mSubPage.setSiteSettingsDelegate(mDelegate.getSiteSettingsDelegate());
        fragmentManager.beginTransaction().add(mSubPage, null).commitNow();
        return mSubPage.requireView();
    }

    /** Removes the last added preference fragment. */
    protected void removeSubpageFragment() {
        assert mSubPage != null;
        FragmentManager fragmentManager = mDelegate.getFragmentManager();
        BaseSiteSettingsFragment subPage = mSubPage;
        mSubPage = null;
        // If the activity is getting destroyed or saved, it is not allowed to modify fragments.
        if (fragmentManager == null || fragmentManager.isStateSaved()) return;
        fragmentManager.beginTransaction().remove(subPage).commitNow();
    }

    /** @return Whether it is possible to add preference fragments. */
    protected boolean canCreateSubpageFragment() {
        return !mDelegate.getFragmentManager().isStateSaved();
    }
}
