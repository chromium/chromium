// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.content_settings.PrefNames.IN_CONTEXT_COOKIE_CONTROLS_OPENED;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteDataCleaner;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePermissionsFetcher;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.content_settings.CookieControlsState;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.Collection;

/** Class for controlling the page info cookies section. */
@NullMarked
public class PageInfoCookiesController extends PageInfoPreferenceSubpageController
        implements CookieControlsObserver {
    private final PageInfoMainController mMainController;
    private final PageInfoRowView mRowView;
    private final String mFullUrl;
    private String mTitle;
    private @Nullable CookieControlsBridge mBridge;
    private @Nullable PageInfoCookiesSettings mSubPage;
    private int mControlsState;
    private int mEnforcement;
    private long mExpiration;
    private boolean mShouldDisplaySiteBreakageString;
    private @Nullable Website mWebsite;
    private final boolean mBlockAll3pc;
    private boolean mIsIncognito;
    private boolean mIsModeBUi;
    private final boolean mIsSiteSettingsAvailable;
    private @Nullable Integer mDaysUntilExpirationForTesting;
    private @Nullable Collection<Website> mRwsInfoForTesting;

    public PageInfoCookiesController(
            PageInfoMainController mainController,
            PageInfoRowView rowView,
            PageInfoControllerDelegate delegate) {
        super(delegate);

        mBlockAll3pc = delegate.allThirdPartyCookiesBlockedTrackingProtection();
        mIsIncognito = delegate.isIncognito();

        mIsModeBUi = delegate.showTrackingProtectionUi();

        mMainController = mainController;
        mRowView = rowView;
        mFullUrl = mainController.getURL().getSpec();
        mIsSiteSettingsAvailable = delegate.isSiteSettingsAvailable();
        mBridge = delegate.createCookieControlsBridge(this);

        mShouldDisplaySiteBreakageString = false;

        updateRowParams();
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
    public @Nullable View createViewForSubpage(ViewGroup parent) {
        assert mSubPage == null;
        if (!canCreateSubpageFragment()) return null;

        mSubPage = new PageInfoCookiesSettings();
        View view = addSubpageFragment(mSubPage);
        var delegate = getDelegate();
        PageInfoCookiesSettings.PageInfoCookiesViewParams params =
                new PageInfoCookiesSettings.PageInfoCookiesViewParams(
                        /* onThirdPartyCookieToggleChanged= */ this
                                ::onThirdPartyCookieToggleChanged,
                        /* onClearCallback= */ this::onClearCookiesClicked,
                        /* onCookieSettingsLinkClicked= */ delegate::showCookieSettings,
                        /* onFeedbackLinkClicked= */ delegate::showCookieFeedback,
                        /* disableCookieDeletion= */ isDeletionDisabled(),
                        /* hostName= */ mMainController.getURL().getHost(),
                        /* blockAll3pc= */ mBlockAll3pc,
                        /* isIncognito= */ mIsIncognito,
                        /* isModeBUi= */ mIsModeBUi,
                        /* daysUntilExpirationForTesting= */ mDaysUntilExpirationForTesting);
        mSubPage.setParams(params, delegate);
        mSubPage.updateState(mControlsState, mEnforcement, mExpiration);

        SiteSettingsCategory storageCategory =
                SiteSettingsCategory.createFromType(
                        mMainController.getBrowserContext(), SiteSettingsCategory.Type.USE_STORAGE);
        new WebsitePermissionsFetcher(delegate.getSiteSettingsDelegate())
                .fetchPreferencesForCategoryAndPopulateRwsInfo(
                        storageCategory, this::onStorageFetched);
        if (mRwsInfoForTesting != null) {
            onStorageFetched(mRwsInfoForTesting);
        }

        return view;
    }

    private void onStorageFetched(Collection<Website> result) {
        String origin = Origin.createOrThrow(mFullUrl).toString();
        WebsiteAddress address = WebsiteAddress.create(origin);
        assert address != null;

        mWebsite =
                SingleWebsiteSettings.mergePermissionAndStorageInfoForTopLevelOrigin(
                        address, result);
        if (mSubPage != null) {
            mSubPage.setStorageUsage(mWebsite.getTotalUsage());

            boolean isRwsInfoShown =
                    mSubPage.maybeShowRwsInfo(
                            mWebsite.getRwsCookieInfo(), mWebsite.getAddress().getOrigin());
            RecordHistogram.recordBooleanHistogram(
                    "Security.PageInfo.Cookies.HasFPSInfo", isRwsInfoShown);
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
    public void updateSubpageIfNeeded() {}

    @Override
    public void onSubpageRemoved() {
        mSubPage = null;
        removeSubpageFragment();
    }

    @Override
    public void onStatusChanged(
            int controlsState, int enforcement, int blockingStatus, long expiration) {
        mControlsState = controlsState;
        mEnforcement = enforcement;
        mExpiration = expiration;
        updateRowParams();

        if (mSubPage != null) {
            mSubPage.updateState(mControlsState, mEnforcement, mExpiration);
        }
    }

    @Override
    public void onHighlightCookieControl(boolean shouldHighlight) {
        if (!mIsModeBUi) {
            mShouldDisplaySiteBreakageString = shouldHighlight;
        }
        mRowView.updateSubtitle(getRowViewSubtitle());
    }

    private boolean isDeletionDisabled() {
        return WebsitePreferenceBridge.isCookieDeletionDisabled(
                mMainController.getBrowserContext(), mFullUrl);
    }

    private void updateRowParams() {
        PageInfoRowView.ViewParams rowParams = new PageInfoRowView.ViewParams();
        rowParams.visible = mIsSiteSettingsAvailable;

        mTitle = mRowView.getContext().getString(R.string.page_info_cookies_title);
        rowParams.title = mTitle;
        rowParams.iconResId = R.drawable.permission_cookie;
        rowParams.decreaseIconSize = true;
        rowParams.clickCallback = this::launchSubpage;
        rowParams.subtitle = getRowViewSubtitle();
        mRowView.setParams(rowParams);
    }

    private @Nullable CharSequence getRowViewSubtitle() {
        var context = mRowView.getContext();
        if (mEnforcement == CookieControlsEnforcement.ENFORCED_BY_TPCD_GRANT
                || mControlsState == CookieControlsState.ALLOWED3PC) {
            return context.getString(R.string.page_info_cookies_subtitle_allowed);
        } else if (mControlsState == CookieControlsState.BLOCKED3PC) {
            if (mIsModeBUi && !mBlockAll3pc) {
                return context.getString(
                        R.string.page_info_tracking_protection_subtitle_cookies_limited);
            } else if (!mIsModeBUi && mShouldDisplaySiteBreakageString) {
                return context.getString(
                        R.string.page_info_cookies_subtitle_blocked_high_confidence);
            }
            return context.getString(R.string.page_info_cookies_subtitle_blocked);
        }
        // Tracking protections UI and 3PC controls hidden UI have no subtitle.
        return null;
    }

    public void setDaysUntilExpirationForTesting(Integer days) {
        mDaysUntilExpirationForTesting = days;
    }

    public void setEnforcementForTesting(@CookieControlsEnforcement int enforcement) {
        mEnforcement = enforcement;
    }

    public void setIsIncognitoForTesting(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    public void setIsModeBUiForTesting(boolean isModeBUi) {
        mIsModeBUi = isModeBUi;
    }

    public void setControlsStateForTesting(@CookieControlsState int controlsState) {
        mControlsState = controlsState;
    }

    public void setRwsInfoForTesting(Collection<Website> rwsInfoForTesting) {
        mRwsInfoForTesting = rwsInfoForTesting;
    }

    void destroy() {
        assumeNonNull(mBridge);
        mBridge.onUiClosing();
        mBridge.destroy();
        mBridge = null;
    }
}
