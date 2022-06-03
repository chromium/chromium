// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.FragmentManager;

import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteDataCleaner;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Collection;

/**
 * Class for controlling the page info cookies section.
 */
public class PageInfoCookiesController
        implements PageInfoSubpageController, CookieControlsObserver {
    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final PageInfoControllerDelegate mDelegate;
    private final String mFullUrl;
    private final String mTitle;
    private CookieControlsBridge mBridge;
    private PageInfoCookiesPreference mSubPage;

    private int mAllowedCookies;
    private int mBlockedCookies;
    private int mStatus;
    private boolean mIsEnforced;
    private Website mWebsite;

    public PageInfoCookiesController(PageInfoMainController mainController, PageInfoRowView rowView,
            PageInfoControllerDelegate delegate) {
        mMainController = mainController;
        mRowView = rowView;
        mDelegate = delegate;
        mFullUrl = mainController.getURL().getSpec();
        mTitle = mRowView.getContext().getResources().getString(R.string.cookies_title);
        mBridge = mDelegate.createCookieControlsBridge(this);

        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = delegate.isSiteSettingsAvailable();
        rowParams.title = mTitle;
        rowParams.iconResId = R.drawable.permission_cookie;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
    }

    private void launchSubpage() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_COOKIES_DIALOG_OPENED);
        mMainController.launchSubpage(this);
    }

    @Override
    public String getSubpageTitle() {
        return mTitle;
    }

    @Override
    public View createViewForSubpage(ViewGroup parent) {
        assert mSubPage == null;

        FragmentManager fragmentManager = mDelegate.getFragmentManager();
        // If the activity is getting destroyed or saved, it is not allowed to modify fragments.
        if (fragmentManager.isStateSaved()) return null;

        mSubPage = new PageInfoCookiesPreference();
        mSubPage.setSiteSettingsDelegate(mDelegate.getSiteSettingsDelegate());
        fragmentManager.beginTransaction().add(mSubPage, null).commitNow();

        PageInfoCookiesPreference.PageInfoCookiesViewParams params =
                new PageInfoCookiesPreference.PageInfoCookiesViewParams();
        params.thirdPartyCookieBlockingEnabled = mDelegate.cookieControlsShown();
        params.onCheckedChangedCallback = this::onCheckedChangedCallback;
        params.onClearCallback = this::onClearCookiesClicked;
        params.onCookieSettingsLinkClicked = mDelegate::showCookieSettings;
        params.disableCookieDeletion = isDeletionDisabled();
        params.hostName = mMainController.getURL().getHost();
        mSubPage.setParams(params);
        mSubPage.setCookiesCount(mAllowedCookies, mBlockedCookies);
        mSubPage.setCookieBlockingStatus(mStatus, mIsEnforced);

        SiteSettingsCategory storageCategory = SiteSettingsCategory.createFromType(
                mMainController.getBrowserContext(), SiteSettingsCategory.Type.USE_STORAGE);
        new WebsitePermissionsFetcher(mMainController.getBrowserContext())
                .fetchPreferencesForCategory(storageCategory, this::onStorageFetched);

        return mSubPage.requireView();
    }

    private void onStorageFetched(Collection<Website> result) {
        String origin = Origin.createOrThrow(mFullUrl).toString();
        WebsiteAddress address = WebsiteAddress.create(origin);

        mWebsite = SingleWebsiteSettings.mergePermissionAndStorageInfoForTopLevelOrigin(
                address, result);
        if (mSubPage != null) {
            mSubPage.setStorageUsage(mWebsite.getTotalUsage());
        }
    }

    private void onCheckedChangedCallback(boolean state) {
        if (mBridge != null) {
            mMainController.recordAction(state ? PageInfoAction.PAGE_INFO_COOKIES_BLOCKED_FOR_SITE
                                               : PageInfoAction.PAGE_INFO_COOKIES_ALLOWED_FOR_SITE);
            mBridge.setThirdPartyCookieBlockingEnabledForSite(state);
        }
    }

    private void onClearCookiesClicked() {
        mMainController.recordAction(PageInfoAction.PAGE_INFO_COOKIES_CLEARED);
        clearData();
    }

    @Override
    public void clearData() {
        if (isDeletionDisabled()) return;
        if (mWebsite == null) return;

        new SiteDataCleaner().clearData(
                mMainController.getBrowserContext(), mWebsite, mMainController::exitSubpage);
    }

    @Override
    public void updateRowIfNeeded() {}

    @Override
    public void onSubpageRemoved() {
        assert mSubPage != null;
        FragmentManager fragmentManager = mDelegate.getFragmentManager();
        PageInfoCookiesPreference subPage = mSubPage;
        mSubPage = null;
        // If the activity is getting destroyed or saved, it is not allowed to modify fragments.
        if (fragmentManager == null || fragmentManager.isStateSaved()) return;
        fragmentManager.beginTransaction().remove(subPage).commitNow();
    }

    @Override
    public void onCookiesCountChanged(int allowedCookies, int blockedCookies) {
        mAllowedCookies = allowedCookies;
        mBlockedCookies = blockedCookies;
        String subtitle = blockedCookies > 0
                ? mRowView.getContext().getResources().getQuantityString(
                        R.plurals.cookie_controls_blocked_cookies, blockedCookies, blockedCookies)
                : null;

        mRowView.updateSubtitle(subtitle);

        if (mSubPage != null) {
            mSubPage.setCookiesCount(allowedCookies, blockedCookies);
        }
    }

    @Override
    public void onCookieBlockingStatusChanged(int status, int enforcement) {
        mStatus = status;
        mIsEnforced = enforcement != CookieControlsEnforcement.NO_ENFORCEMENT;
        if (mSubPage != null) {
            mSubPage.setCookieBlockingStatus(mStatus, mIsEnforced);
        }
    }

    private boolean isDeletionDisabled() {
        return WebsitePreferenceBridge.isCookieDeletionDisabled(mMainController.getBrowserContext(), mFullUrl);
    }

    void onUiClosing() {
        if (mBridge != null) {
            mBridge.onUiClosing();
        }
    }

    void destroy() {
        mBridge.destroy();
        mBridge = null;
    }
}
