// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.res.Resources;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;

import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;

import java.util.List;

/**
 * Class for controlling the page info permissions section.
 */
public class PageInfoPermissionsController
        implements PageInfoSubpageController, SingleWebsiteSettings.Observer {
    private PageInfoMainController mMainController;
    private PageInfoRowView mRowView;
    private PageInfoControllerDelegate mDelegate;
    private String mTitle;
    private String mPageUrl;
    private SingleWebsiteSettings mSubpageFragment;

    public PageInfoPermissionsController(PageInfoMainController mainController,
            PageInfoRowView view, PageInfoControllerDelegate delegate, String pageUrl) {
        mMainController = mainController;
        mRowView = view;
        mDelegate = delegate;
        mPageUrl = pageUrl;
    }

    private void launchSubpage() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_PERMISSION_DIALOG_OPENED);
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
        mSubpageFragment.setWebsiteSettingsObserver(this);
        AppCompatActivity host = (AppCompatActivity) mRowView.getContext();
        host.getSupportFragmentManager().beginTransaction().add(mSubpageFragment, null).commitNow();
        return mSubpageFragment.requireView();
    }

    @Override
    public void onSubpageRemoved() {
        assert mSubpageFragment != null;
        AppCompatActivity host = (AppCompatActivity) mRowView.getContext();
        host.getSupportFragmentManager().beginTransaction().remove(mSubpageFragment).commitNow();
        mSubpageFragment = null;
    }

    public void setPermissions(PageInfoView.PermissionParams params) {
        Resources resources = mRowView.getContext().getResources();
        mTitle = resources.getString(R.string.page_info_permissions_title);
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.title = mTitle;
        rowParams.iconResId = R.drawable.ic_tune_24dp;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;
        rowParams.subtitle = getPermissionSummaryString(params.permissions, resources);
        rowParams.visible = rowParams.subtitle != null;
        mRowView.setParams(rowParams);
    }

    /**
     * Returns the most comprehensive subtitle summary string.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static String getPermissionSummaryString(
            List<PageInfoView.PermissionRowParams> permissions, Resources resources) {
        int numPermissions = permissions.size();
        if (numPermissions == 0) {
            return null;
        }

        PageInfoView.PermissionRowParams perm1 = permissions.get(0);
        boolean same = true;
        for (PageInfoView.PermissionRowParams perm : permissions) {
            if (perm.warningTextResource != 0) {
                // Show the first (most important warning) only, if there is one.
                return resources.getString(R.string.page_info_permissions_os_warning,
                        perm.name.toString(), resources.getString(perm.warningTextResource));
            }
            // Whether all permissions have had the same status so far.
            same = same && (perm1.allowed == perm.allowed);
        }

        if (numPermissions == 1) {
            int resId = perm1.allowed ? R.string.page_info_permissions_summary_1_allowed
                                      : R.string.page_info_permissions_summary_1_blocked;
            return resources.getString(resId, perm1.name.toString());
        }

        PageInfoView.PermissionRowParams perm2 = permissions.get(1);
        if (numPermissions == 2) {
            if (same) {
                int resId = perm1.allowed ? R.string.page_info_permissions_summary_2_allowed
                                          : R.string.page_info_permissions_summary_2_blocked;
                return resources.getString(resId, perm1.name.toString(), perm2.name.toString());
            }
            int resId = R.string.page_info_permissions_summary_2_mixed;
            // Put the allowed permission first.
            return resources.getString(resId,
                    perm1.allowed ? perm1.name.toString() : perm2.name.toString(),
                    perm1.allowed ? perm2.name.toString() : perm1.name.toString());
        }

        // More than 2 permissions.
        if (same) {
            int resId = perm1.allowed ? R.plurals.page_info_permissions_summary_more_allowed
                                      : R.plurals.page_info_permissions_summary_more_blocked;
            return resources.getQuantityString(resId, numPermissions - 2, perm1.name.toString(),
                    perm2.name.toString(), numPermissions - 2);
        }
        int resId = R.plurals.page_info_permissions_summary_more_mixed;
        return resources.getQuantityString(resId, numPermissions - 2, perm1.name.toString(),
                perm2.name.toString(), numPermissions - 2);
    }

    // SingleWebsiteSettings.Observer methods

    @Override
    public void onPermissionsReset() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_PERMISSIONS_CLEARED);
        mMainController.refreshPermissions();
        mMainController.exitSubpage();
    }

    @Override
    public void onPermissionChanged() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_PERMISSIONS_CHANGED);
        mMainController.refreshPermissions();
    }
}
