// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;

import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;

/**
 * Class for controlling the page info permissions section.
 */
public class PageInfoPermissionsController implements PageInfoSubpageController {
    private PageInfoMainPageController mMainController;
    private PageInfoRowView mRowView;
    private PageInfoControllerDelegate mDelegate;
    private String mTitle;
    private String mPageUrl;
    private SingleWebsiteSettings mSubpageFragment;

    public PageInfoPermissionsController(PageInfoMainPageController mainController,
            PageInfoRowView view, PageInfoControllerDelegate delegate, String pageUrl) {
        mMainController = mainController;
        mRowView = view;
        mDelegate = delegate;
        mPageUrl = pageUrl;
    }

    private void launchSubpage() {
        mMainController.launchSubpage(this);
    }

    @Override
    public String getSubpageTitle() {
        return mTitle;
    }

    @Override
    public View createViewForSubpage(ViewGroup parent) {
        assert mSubpageFragment == null;
        Bundle fragmentArgs = SingleWebsiteSettings.createFragmentArgsForSite(mPageUrl);
        mSubpageFragment = (SingleWebsiteSettings) Fragment.instantiate(
                mRowView.getContext(), SingleWebsiteSettings.class.getName(), fragmentArgs);
        mSubpageFragment.setSiteSettingsClient(mDelegate.getSiteSettingsClient());
        mSubpageFragment.setHideNonPermissionPreferences(true);
        mSubpageFragment.setRefreshAfterReset(true);
        AppCompatActivity host = (AppCompatActivity) mRowView.getContext();
        host.getSupportFragmentManager().beginTransaction().add(mSubpageFragment, null).commitNow();
        return mSubpageFragment.getView();
    }

    @Override
    public void onSubPageAttached() {}

    @Override
    public void onSubpageRemoved() {
        AppCompatActivity host = (AppCompatActivity) mRowView.getContext();
        host.getSupportFragmentManager().beginTransaction().remove(mSubpageFragment).commitNow();
        mSubpageFragment = null;
    }

    public void setPermissions(PageInfoView.PermissionParams params) {
        mTitle = mRowView.getContext().getResources().getString(
                R.string.page_info_permissions_title);
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = true;
        rowParams.title = mTitle;
        rowParams.iconResId = R.drawable.ic_tune_24dp;
        // TODO(crbug.com/1077766): Create a permissions subtitle string that represents
        // the state, using the PageInfoView.PermissionParams and potentially R.plurals.
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
    }
}
