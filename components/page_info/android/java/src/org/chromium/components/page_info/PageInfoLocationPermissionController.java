// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.res.Resources;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.site_settings.LocationPermissionSubpageSettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.embedder_support.util.Origin;

/** Class for controlling the page info location permission subpage. */
@NullMarked
public class PageInfoLocationPermissionController extends PageInfoPreferenceSubpageController {
    private final PageInfoRowView mRowView;
    private final String mPageUrl;
    private @Nullable LocationPermissionSubpageSettings mSubPage;

    public PageInfoLocationPermissionController(
            PageInfoRowView view, PageInfoControllerDelegate delegate, String pageUrl) {
        super(delegate);
        mRowView = view;
        mPageUrl = pageUrl;
    }

    @Override
    public String getSubpageTitle() {
        Resources resources = mRowView.getContext().getResources();
        return resources.getString(R.string.website_settings_device_location);
    }

    @Override
    public @Nullable View createViewForSubpage(ViewGroup parent) {
        assert mSubPage == null;
        if (!canCreateSubpageFragment()) return null;

        Bundle fragmentArgs = new Bundle();
        String origin = Origin.createOrThrow(mPageUrl).toString();
        WebsiteAddress address = WebsiteAddress.create(origin);
        fragmentArgs.putSerializable(SingleWebsiteSettings.EXTRA_SITE_ADDRESS, address);

        mSubPage =
                (LocationPermissionSubpageSettings)
                        Fragment.instantiate(
                                mRowView.getContext(),
                                LocationPermissionSubpageSettings.class.getName(),
                                fragmentArgs);
        return addSubpageFragment(mSubPage);
    }

    @Override
    public void onSubpageRemoved() {
        removeSubpageFragment();
        mSubPage = null;
    }

    @Override
    public void clearData() {}

    @Override
    public void updateRowIfNeeded() {}

    @Override
    public void updateSubpageIfNeeded() {}
}
