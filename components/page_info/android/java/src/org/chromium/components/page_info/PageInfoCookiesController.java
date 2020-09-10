// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.app.AppCompatActivity;

import org.chromium.components.browser_ui.site_settings.SiteDataCleaner;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.util.Origin;

/**
 * Class for controlling the page info cookies section.
 */
public class PageInfoCookiesController
        implements PageInfoSubpageController, CookieControlsObserver {
    private PageInfoMainController mMainController;
    private PageInfoRowView mRowView;
    private CookieControlsBridge mBridge;
    private PageInfoControllerDelegate mDelegate;
    private String mFullUrl;
    private String mTitle;
    private PageInfoCookiesPreference mSubPage;

    private int mAllowedCookies;
    private int mBlockedCookies;
    private int mStatus;
    private boolean mIsEnforced;

    public PageInfoCookiesController(PageInfoMainController mainController, PageInfoRowView rowView,
            PageInfoControllerDelegate delegate, boolean isVisible, String fullUrl) {
        mMainController = mainController;
        mRowView = rowView;
        mDelegate = delegate;
        mFullUrl = fullUrl;
        mTitle = mRowView.getContext().getResources().getString(R.string.cookies_title);

        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = isVisible;
        rowParams.title = mTitle;
        rowParams.iconResId = R.drawable.permission_cookie;
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
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
        assert mSubPage == null;
        mSubPage = new PageInfoCookiesPreference();
        PageInfoCookiesPreference.PageInfoCookiesViewParams params =
                new PageInfoCookiesPreference.PageInfoCookiesViewParams();
        params.onCheckedChangedCallback = this::onCheckedChangedCallback;
        params.onClearCallback = this::clearData;
        params.onCookieSettingsLinkClicked = mDelegate::showCookieSettings;
        params.disableCookieDeletion = WebsitePreferenceBridge.isCookieDeletionDisabled(
                mMainController.getBrowserContext(), mFullUrl);
        mSubPage.setParams(params);

        AppCompatActivity host = (AppCompatActivity) mRowView.getContext();
        host.getSupportFragmentManager().beginTransaction().add(mSubPage, "FOO").commitNow();
        return mSubPage.requireView();
    }

    @Override
    public void onSubPageAttached() {
        // TODO(crbug.com/1077766): Get storage size.
        mSubPage.setCookiesCount(mAllowedCookies, mBlockedCookies);
        mSubPage.setCookieBlockingStatus(mStatus, mIsEnforced);
    }

    private void onCheckedChangedCallback(boolean state) {
        mBridge.setThirdPartyCookieBlockingEnabledForSite(state);
    }

    private void clearData() {
        String origin = Origin.createOrThrow(mFullUrl).toString();
        WebsiteAddress address = WebsiteAddress.create(origin);
        new SiteDataCleaner().clearData(mMainController.getBrowserContext(),
                new Website(address, address), mMainController::exitSubpage);
    }

    @Override
    public void onSubpageRemoved() {
        AppCompatActivity host = (AppCompatActivity) mRowView.getContext();
        host.getSupportFragmentManager().beginTransaction().remove(mSubPage).commitNow();
        mSubPage = null;
    }

    @Override
    public void onCookiesCountChanged(int allowedCookies, int blockedCookies) {
        mAllowedCookies = allowedCookies;
        mBlockedCookies = blockedCookies;
        String subtitle = blockedCookies > 0
                ? mRowView.getContext().getResources().getQuantityString(
                        R.plurals.cookie_controls_blocked_cookies, blockedCookies, blockedCookies)
                : mRowView.getContext().getResources().getQuantityString(
                        R.plurals.page_info_cookies_in_use, allowedCookies, allowedCookies);
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

    public void setCookieControlsBridge(CookieControlsBridge cookieBridge) {
        mBridge = cookieBridge;
    }
}
