// Copyright 2024 The Chromium Authors
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
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsBridge.TrackingProtectionFeature;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.Collection;
import java.util.List;

/** Class for controlling the page info tracking protection section for 3PCD 100% launch. */
public class PageInfoTrackingProtectionLaunchController extends PageInfoPreferenceSubpageController
        implements CookieControlsObserver {
    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final String mFullUrl;
    private final String mTitle;
    private CookieControlsBridge mBridge;
    private PageInfoTrackingProtectionLaunchSettings mSubPage;

    private boolean mCookieControlsVisible;
    private boolean mProtectionsOn;
    private List<TrackingProtectionFeature> mFeatures;
    private long mExpiration;
    private Website mWebsite;
    private boolean mBlockAll3PC;
    private boolean mIsIncognito;
    private boolean mFixedExpirationForTesting;

    public PageInfoTrackingProtectionLaunchController(
            PageInfoMainController mainController,
            PageInfoRowView rowView,
            PageInfoControllerDelegate delegate) {
        super(delegate);

        mBlockAll3PC = delegate.allThirdPartyCookiesBlockedTrackingProtection();
        mIsIncognito = delegate.isIncognito();

        mMainController = mainController;
        mRowView = rowView;
        mFullUrl = mainController.getURL().getSpec();
        mTitle =
                mRowView.getContext()
                        .getResources()
                        .getString(R.string.page_info_tracking_protection_title);
        mBridge = delegate.createCookieControlsBridge(this);

        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = delegate.isSiteSettingsAvailable();
        rowParams.title = mTitle;
        rowParams.iconResId = R.drawable.ic_eye_crossed;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;
        mRowView.setParams(rowParams);
        updateRowViewSubtitle();
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

        mSubPage = new PageInfoTrackingProtectionLaunchSettings();
        View view = addSubpageFragment(mSubPage);
        PageInfoTrackingProtectionLaunchSettings.PageInfoTrackingProtectionLaunchViewParams params =
                new PageInfoTrackingProtectionLaunchSettings
                        .PageInfoTrackingProtectionLaunchViewParams();
        params.thirdPartyCookieBlockingEnabled = getDelegate().cookieControlsShown();
        params.onThirdPartyCookieToggleChanged = this::onThirdPartyCookieToggleChanged;
        params.onClearCallback = this::onClearCookiesClicked;
        params.onCookieSettingsLinkClicked = getDelegate()::showTrackingProtectionSettings;
        params.onFeedbackLinkClicked = getDelegate()::showCookieFeedback;
        params.disableCookieDeletion = isDeletionDisabled();
        params.hostName = mMainController.getURL().getHost();
        params.blockAll3PC = mBlockAll3PC;
        params.isIncognito = mIsIncognito;
        params.fixedExpirationForTesting = mFixedExpirationForTesting;
        mSubPage.setParams(params);
        mSubPage.setTrackingProtectionStatus(
                mCookieControlsVisible, mProtectionsOn, mExpiration, mFeatures);

        SiteSettingsCategory storageCategory =
                SiteSettingsCategory.createFromType(
                        mMainController.getBrowserContext(), SiteSettingsCategory.Type.USE_STORAGE);
        new WebsitePermissionsFetcher(getDelegate().getSiteSettingsDelegate())
                .fetchPreferencesForCategoryAndPopulateRwsInfo(
                        storageCategory, this::onStorageFetched);

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

            boolean isRWSInfoShown =
                    mSubPage.maybeShowRWSInfo(
                            mWebsite.getRWSCookieInfo(), mWebsite.getAddress().getOrigin());
            RecordHistogram.recordBooleanHistogram(
                    "Security.PageInfo.Cookies.HasFPSInfo", isRWSInfoShown);
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
                getDelegate().getSiteSettingsDelegate(), mWebsite, mMainController::exitSubpage);
    }

    @Override
    public void updateRowIfNeeded() {}

    @Override
    public void onSubpageRemoved() {
        mSubPage = null;
        removeSubpageFragment();
    }

    @Override
    public void onTrackingProtectionStatusChanged(
            boolean controlsVisible,
            boolean protectionsOn,
            long expiration,
            List<TrackingProtectionFeature> features) {
        mCookieControlsVisible = controlsVisible;
        mProtectionsOn = protectionsOn;
        mExpiration = expiration;
        mFeatures = features;

        updateRowViewSubtitle();

        if (mSubPage != null) {
            mSubPage.setTrackingProtectionStatus(
                    controlsVisible, protectionsOn, expiration, features);
        }
    }

    @Override
    public void onHighlightCookieControl(boolean shouldHighlight) {
        updateRowViewSubtitle();
    }

    private boolean isDeletionDisabled() {
        return WebsitePreferenceBridge.isCookieDeletionDisabled(
                mMainController.getBrowserContext(), mFullUrl);
    }

    private void updateRowViewSubtitle() {
        if (!mCookieControlsVisible) return;
        if (!mProtectionsOn) {
            mRowView.updateSubtitle(
                    mRowView.getContext().getString(R.string.page_info_cookies_subtitle_allowed));
            return;
        }
        mRowView.updateSubtitle(
                mRowView.getContext()
                        .getString(
                                mBlockAll3PC
                                        ? R.string.page_info_cookies_subtitle_blocked
                                        : R.string
                                                .page_info_tracking_protection_subtitle_cookies_limited));
    }

    public void setFixedExceptionExpirationForTesting(boolean fixed) {
        mFixedExpirationForTesting = fixed;
    }

    void destroy() {
        mBridge.onUiClosing();
        mBridge.destroy();
        mBridge = null;
    }
}
