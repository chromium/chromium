// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;

import androidx.fragment.app.FragmentManager;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;

/** Abstract class for controllers that use a BaseSiteSettingsFragment as subpage. */
@NullMarked
public abstract class PageInfoPreferenceSubpageController implements PageInfoSubpageController {
    private final PageInfoControllerDelegate mDelegate;
    private @Nullable BaseSiteSettingsFragment mSubPage;

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
    protected @Nullable View addSubpageFragment(BaseSiteSettingsFragment fragment) {
        assert mSubPage == null;

        // If the activity is getting destroyed or saved, it is not allowed to modify fragments.
        if (!canCreateSubpageFragment()) return null;

        FragmentManager fragmentManager = assumeNonNull(mDelegate.getFragmentManager());
        mSubPage = fragment;
        mSubPage.setSiteSettingsDelegate(mDelegate.getSiteSettingsDelegate());
        fragmentManager.beginTransaction().add(mSubPage, null).commitNow();
        return mSubPage.requireView();
    }

    /** Removes the last added preference fragment. */
    protected void removeSubpageFragment() {
        if (mSubPage == null) return;
        BaseSiteSettingsFragment subPage = mSubPage;
        mSubPage = null;
        // If the activity is getting destroyed or saved, it is not allowed to modify fragments.
        if (!canCreateSubpageFragment()) return;

        FragmentManager fragmentManager = assumeNonNull(mDelegate.getFragmentManager());
        fragmentManager.beginTransaction().remove(subPage).commitNow();
    }

    /**
     * @return Whether it is possible to add preference fragments.
     */
    protected boolean canCreateSubpageFragment() {
        FragmentManager fragmentManager = mDelegate.getFragmentManager();
        return fragmentManager != null
                && !fragmentManager.isStateSaved()
                && !fragmentManager.isDestroyed();
    }

    @Override
    public @Nullable View getCurrentSubpageView() {
        return mSubPage != null ? mSubPage.requireView() : null;
    }
}
