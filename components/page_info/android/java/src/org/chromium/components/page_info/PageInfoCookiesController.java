// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import static org.chromium.components.content_settings.PrefNames.IN_CONTEXT_COOKIE_CONTROLS_OPENED;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteDataCleaner;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.components.content_settings.CookieControlsBreakageConfidenceLevel;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.content_settings.CookieControlsStatus;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.Collection;

/** Class for controlling the page info cookies section. */
public class PageInfoCookiesController extends PageInfoPreferenceSubpageController
        implements CookieControlsObserver {
    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final String mFullUrl;
    private final String mTitle;
    private CookieControlsBridge mBridge;
    private PageInfoCookiesSettings mSubPage;

    private int mAllowedCookies;
    private int mBlockedCookies;
    private int mAllowedSites;
    private int mBlockedSites;
    private int mStatus;
    private int mEnforcement;
    private boolean mIsEnforced;
    private long mExpiration;
    private int mConfidenceLevel;
    private Website mWebsite;
    private boolean mTrackingProtectionUI;
    private boolean mBlockAll3PC;
    private boolean mIsIncognito;

    public PageInfoCookiesController(
            PageInfoMainController mainController,
            PageInfoRowView rowView,
            PageInfoControllerDelegate delegate) {
        super(delegate);

        mTrackingProtectionUI =
                PageInfoFeatures.USER_BYPASS_UI.isEnabled() && delegate.showTrackingProtectionUI();
        mBlockAll3PC = delegate.allThirdPartyCookiesBlockedTrackingProtection();
        mIsIncognito = delegate.isIncognito();

        mMainController = mainController;
        mRowView = rowView;
        mFullUrl = mainController.getURL().getSpec();
        mTitle =
                mRowView.getContext()
                        .getResources()
                        .getString(
                                mTrackingProtectionUI
                                        ? R.string.page_info_tracking_protection_title
                                        : R.string.page_info_cookies_title);
        mBridge = delegate.createCookieControlsBridge(this);

        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = delegate.isSiteSettingsAvailable();
        rowParams.title = mTitle;
        rowParams.iconResId =
                mTrackingProtectionUI ? R.drawable.ic_eye_crossed : R.drawable.permission_cookie;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
        if (PageInfoFeatures.USER_BYPASS_UI.isEnabled()) {
            // Need to get the status and confidence level synchronously since the callbacks are
            // only invoked when those change.
            mStatus = mBridge.getCookieControlsStatus();
            mConfidenceLevel = mBridge.getBreakageConfidenceLevel();
            updateRowViewSubtitle();
        }
    }

    private void launchSubpage() {
        // Record a pref on page open if 3PC blocking is enabled.
        if (getDelegate().cookieControlsShown()) {
            UserPrefs.get(mMainController.getBrowserContext())
                    .setBoolean(IN_CONTEXT_COOKIE_CONTROLS_OPENED, true);
        }
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
        if (!canCreateSubpageFragment()) return null;

        mSubPage = new PageInfoCookiesSettings();
        View view = addSubpageFragment(mSubPage);
        PageInfoCookiesSettings.PageInfoCookiesViewParams params =
                new PageInfoCookiesSettings.PageInfoCookiesViewParams();
        params.thirdPartyCookieBlockingEnabled = getDelegate().cookieControlsShown();
        params.onThirdPartyCookieToggleChanged = this::onThirdPartyCookieToggleChanged;
        params.onClearCallback = this::onClearCookiesClicked;
        params.onCookieSettingsLinkClicked =
                mTrackingProtectionUI
                        ? getDelegate()::showTrackingProtectionSettings
                        : getDelegate()::showCookieSettings;
        params.onFeedbackLinkClicked = getDelegate()::showCookieFeedback;
        params.disableCookieDeletion = isDeletionDisabled();
        params.hostName = mMainController.getURL().getHost();
        params.showTrackingProtectionUI = mTrackingProtectionUI;
        params.blockAll3PC = mBlockAll3PC;
        params.isIncognito = mIsIncognito;
        mSubPage.setParams(params);
        if (PageInfoFeatures.USER_BYPASS_UI.isEnabled()) {
            mSubPage.setCookieStatus(mStatus, mEnforcement, mExpiration);
            mSubPage.setSitesCount(mAllowedSites, mBlockedSites);
        } else {
            mSubPage.setCookieBlockingStatus(mStatus, mIsEnforced);
            mSubPage.setCookiesCount(mAllowedCookies, mBlockedCookies);
        }

        SiteSettingsCategory storageCategory =
                SiteSettingsCategory.createFromType(
                        mMainController.getBrowserContext(), SiteSettingsCategory.Type.USE_STORAGE);
        new WebsitePermissionsFetcher(mMainController.getBrowserContext())
                .fetchPreferencesForCategoryAndPopulateFpsInfo(
                        getDelegate().getSiteSettingsDelegate(),
                        storageCategory,
                        this::onStorageFetched);

        return view;
    }

    private void onStorageFetched(Collection<Website> result) {
        String origin = Origin.createOrThrow(mFullUrl).toString();
        WebsiteAddress address = WebsiteAddress.create(origin);

        mWebsite =
                SingleWebsiteSettings.mergePermissionAndStorageInfoForTopLevelOrigin(
                        address, result);
        if (mSubPage != null) {
            mSubPage.setStorageUsage(mWebsite.getTotalUsage());

            boolean isFPSInfoShown =
                    mSubPage.maybeShowFPSInfo(
                            mWebsite.getFPSCookieInfo(), mWebsite.getAddress().getOrigin());
            RecordHistogram.recordBooleanHistogram(
                    "Security.PageInfo.Cookies.HasFPSInfo", isFPSInfoShown);
        }
    }

    private void onThirdPartyCookieToggleChanged(boolean block) {
        if (mBridge != null) {
            mMainController.recordAction(
                    block
                            ? PageInfoAction.PAGE_INFO_COOKIES_BLOCKED_FOR_SITE
                            : PageInfoAction.PAGE_INFO_COOKIES_ALLOWED_FOR_SITE);
            mBridge.setThirdPartyCookieBlockingEnabledForSite(block);
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

        RecordHistogram.recordEnumeratedHistogram(
                "Privacy.DeleteBrowsingData.Action",
                DeleteBrowsingDataAction.COOKIES_IN_USE_DIALOG,
                DeleteBrowsingDataAction.MAX_VALUE);

        SiteDataCleaner.clearData(
                mMainController.getBrowserContext(), mWebsite, mMainController::exitSubpage);
    }

    @Override
    public void updateRowIfNeeded() {}

    @Override
    public void onSubpageRemoved() {
        mSubPage = null;
        removeSubpageFragment();
    }

    @Override
    public void onCookiesCountChanged(int allowedCookies, int blockedCookies) {
        mAllowedCookies = allowedCookies;
        mBlockedCookies = blockedCookies;
        String subtitle =
                blockedCookies > 0
                        ? mRowView.getContext()
                                .getResources()
                                .getQuantityString(
                                        R.plurals.cookie_controls_blocked_cookies,
                                        blockedCookies,
                                        blockedCookies)
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

    @Override
    public void onStatusChanged(int status, int enforcement, int blockingStatus, long expiration) {
        mStatus = status;
        mEnforcement = enforcement;
        mExpiration = expiration;

        updateRowViewSubtitle();

        if (mSubPage != null) {
            mSubPage.setCookieStatus(mStatus, mEnforcement, expiration);
        }
    }

    @Override
    public void onSitesCountChanged(int allowedSites, int blockedSites) {
        mAllowedSites = allowedSites;
        mBlockedSites = blockedSites;
        if (mSubPage != null) {
            mSubPage.setSitesCount(allowedSites, blockedSites);
        }
    }

    @Override
    public void onBreakageConfidenceLevelChanged(@CookieControlsBreakageConfidenceLevel int level) {
        mConfidenceLevel = level;
        updateRowViewSubtitle();
    }

    private boolean isDeletionDisabled() {
        return WebsitePreferenceBridge.isCookieDeletionDisabled(
                mMainController.getBrowserContext(), mFullUrl);
    }

    private void updateRowViewSubtitle() {
        if (mStatus == CookieControlsStatus.DISABLED) return;
        if (mStatus == CookieControlsStatus.DISABLED_FOR_SITE) {
            mRowView.updateSubtitle(
                    mRowView.getContext().getString(R.string.page_info_cookies_subtitle_allowed));
            return;
        }
        if (mTrackingProtectionUI) {
            mRowView.updateSubtitle(
                    mRowView.getContext()
                            .getString(
                                    mBlockAll3PC
                                            ? R.string.page_info_cookies_subtitle_blocked
                                            : R.string
                                                    .page_info_tracking_protection_subtitle_cookies_limited));
            return;
        }
        mRowView.updateSubtitle(
                mRowView.getContext()
                        .getString(
                                mConfidenceLevel == CookieControlsBreakageConfidenceLevel.HIGH
                                        ? R.string
                                                .page_info_cookies_subtitle_blocked_high_confidence
                                        : R.string.page_info_cookies_subtitle_blocked));
    }

    void destroy() {
        mBridge.onUiClosing();
        mBridge.destroy();
        mBridge = null;
    }
}
