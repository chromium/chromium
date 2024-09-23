// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.res.Resources;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteDataCleaner;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.Collection;
import java.util.List;

/** Class for controlling the page info permissions section. */
public class PageInfoPermissionsController extends PageInfoPreferenceSubpageController
        implements SingleWebsiteSettings.Observer {
    /**  Parameters to represent a single permission. */
    public static class PermissionObject {
        public @ContentSettingsType.EnumType int type;
        public CharSequence name;
        public CharSequence nameMidSentence;
        public boolean allowed;
        public @StringRes int warningTextResource;
    }

    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final String mTitle;
    private final String mPageUrl;
    private boolean mHasSoundPermission;
    private boolean mDataIsStale;
    private SingleWebsiteSettings mSubPage;
    @ContentSettingsType.EnumType private int mHighlightedPermission;
    @ColorRes private int mHighlightColor;

    public PageInfoPermissionsController(
            PageInfoMainController mainController,
            PageInfoRowView view,
            PageInfoControllerDelegate delegate,
            @ContentSettingsType.EnumType int highlightedPermission) {
        super(delegate);
        mMainController = mainController;
        mRowView = view;
        mPageUrl = mainController.getURL().getSpec();
        mHighlightedPermission = highlightedPermission;
        Resources resources = mRowView.getContext().getResources();
        mHighlightColor = R.color.iph_highlight_blue;
        mTitle = resources.getString(R.string.page_info_permissions_title);
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
        assert mSubPage == null;
        if (!canCreateSubpageFragment()) return null;

        Bundle fragmentArgs = SingleWebsiteSettings.createFragmentArgsForSite(mPageUrl);
        fragmentArgs.putBoolean(SingleWebsiteSettings.EXTRA_SHOW_SOUND, mHasSoundPermission);

        mSubPage =
                (SingleWebsiteSettings)
                        Fragment.instantiate(
                                mRowView.getContext(),
                                SingleWebsiteSettings.class.getName(),
                                fragmentArgs);
        mSubPage.setHideNonPermissionPreferences(true);
        mSubPage.setWebsiteSettingsObserver(this);
        if (mHighlightedPermission != ContentSettingsType.DEFAULT) {
            mSubPage.setHighlightedPermission(mHighlightedPermission, mHighlightColor);
        }
        return addSubpageFragment(mSubPage);
    }

    @Override
    public void onSubpageRemoved() {
        removeSubpageFragment();
        mSubPage = null;
    }

    public void setPermissions(List<PermissionObject> permissions) {
        Resources resources = mRowView.getContext().getResources();
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.title = mTitle;
        rowParams.iconResId = R.drawable.ic_tune_24dp;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;
        rowParams.subtitle = getPermissionSummaryString(permissions, resources);
        rowParams.visible = getDelegate().isSiteSettingsAvailable() && rowParams.subtitle != null;
        if (mHighlightedPermission != ContentSettingsType.DEFAULT) {
            rowParams.rowTint = mHighlightColor;
        }
        mRowView.setParams(rowParams);

        mHasSoundPermission = false;
        for (PermissionObject permission : permissions) {
            if (permission.type == ContentSettingsType.SOUND) {
                mHasSoundPermission = true;
                break;
            }
        }
    }

    /** Returns the most comprehensive subtitle summary string. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static String getPermissionSummaryString(
            List<PermissionObject> permissions, Resources resources) {
        int numPermissions = permissions.size();
        if (numPermissions == 0) {
            return null;
        }

        PermissionObject perm1 = permissions.get(0);
        boolean same = true;
        for (PermissionObject perm : permissions) {
            if (perm.warningTextResource != 0) {
                // Show the first (most important warning) only, if there is one.
                return resources.getString(
                        R.string.page_info_permissions_os_warning,
                        perm.name.toString(),
                        resources.getString(perm.warningTextResource));
            }
            // Whether all permissions have had the same status so far.
            same = same && (perm1.allowed == perm.allowed);
        }

        if (numPermissions == 1) {
            int resId =
                    perm1.allowed
                            ? R.string.page_info_permissions_summary_1_allowed
                            : R.string.page_info_permissions_summary_1_blocked;
            return resources.getString(resId, perm1.name.toString());
        }

        PermissionObject perm2 = permissions.get(1);
        if (numPermissions == 2) {
            if (same) {
                int resId =
                        perm1.allowed
                                ? R.string.page_info_permissions_summary_2_allowed
                                : R.string.page_info_permissions_summary_2_blocked;
                return resources.getString(
                        resId, perm1.name.toString(), perm2.nameMidSentence.toString());
            }
            int resId = R.string.page_info_permissions_summary_2_mixed;
            // Put the allowed permission first.
            return resources.getString(
                    resId,
                    perm1.allowed ? perm1.name.toString() : perm2.name.toString(),
                    perm1.allowed
                            ? perm2.nameMidSentence.toString()
                            : perm1.nameMidSentence.toString());
        }

        // More than 2 permissions.
        if (same) {
            int resId =
                    perm1.allowed
                            ? R.plurals.page_info_permissions_summary_more_allowed
                            : R.plurals.page_info_permissions_summary_more_blocked;
            return resources.getQuantityString(
                    resId,
                    numPermissions - 2,
                    perm1.name.toString(),
                    perm2.nameMidSentence.toString(),
                    numPermissions - 2);
        }
        int resId = R.plurals.page_info_permissions_summary_more_mixed;
        return resources.getQuantityString(
                resId,
                numPermissions - 2,
                perm1.name.toString(),
                perm2.nameMidSentence.toString(),
                numPermissions - 2);
    }

    @Override
    public void clearData() {
        RecordHistogram.recordEnumeratedHistogram(
                "Privacy.DeleteBrowsingData.Action",
                DeleteBrowsingDataAction.PAGE_INFO_RESET_PERMISSIONS,
                DeleteBrowsingDataAction.MAX_VALUE);
        // Need to fetch data in order to clear it.
        BrowserContextHandle browserContext = getDelegate().getBrowserContext();
        WebsitePermissionsFetcher fetcher =
                new WebsitePermissionsFetcher(getDelegate().getSiteSettingsDelegate());
        String origin = Origin.createOrThrow(mPageUrl).toString();
        WebsiteAddress address = WebsiteAddress.create(origin);

        // Asynchronous function, callback will clear the data.
        fetcher.fetchAllPreferences(
                (Collection<Website> sites) -> {
                    Website site =
                            SingleWebsiteSettings.mergePermissionAndStorageInfoForTopLevelOrigin(
                                    address, sites);
                    SiteDataCleaner.resetPermissions(browserContext, site);
                    mMainController.refreshPermissions();
                });
    }

    @Override
    public void updateRowIfNeeded() {
        if (mDataIsStale) {
            mMainController.refreshPermissions();
        }
        mDataIsStale = false;
    }

    // SingleWebsiteSettings.Observer methods

    @Override
    public void onPermissionsReset() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_PERMISSIONS_CLEARED);
        mDataIsStale = true;
        mMainController.exitSubpage();
    }

    @Override
    public void onPermissionChanged() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_CHANGED_PERMISSION);
        mDataIsStale = true;
    }
}
