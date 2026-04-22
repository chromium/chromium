// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils.SearchEngineIconObserver;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator.PageInfoAction;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.PermissionIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusView.IconTransitionType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.extensions.ExtensionUi;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.util.DrawableUtils;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput.SiteSearchData;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Contains the controller logic of the Status component. */
@NullMarked
public class StatusMediator
        implements TemplateUrlServiceObserver,
                CookieControlsObserver,
                SearchEngineIconObserver,
                PermissionStatusHandler.Delegate {

    static final String COOKIE_CONTROLS_ICON = "COOKIE_CONTROLS_ICON";

    private final PropertyModel mModel;
    private final OneshotSupplier<TemplateUrlService> mTemplateUrlServiceSupplier;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final boolean mIsTablet;
    private final Context mContext;
    private final LocationBarDataProvider mLocationBarDataProvider;
    private final PermissionStatusHandler mPermissionStatusHandler;
    private final Handler mIconTaskHandler = new Handler();
    private final Handler mStoreIconHandler = new Handler();
    private final PageInfoIphController mPageInfoIphController;
    private final PageInfoAction mPageInfoAction;
    private final NonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier;
    private final OnClickListener mFuseboxOnPlusButtonClicked;
    private final Callback<@Nullable SiteSearchData> mSiteSearchDataObserver =
            this::onSiteSearchDataChanged;
    private final Callback<@FuseboxState Integer> mOnFuseboxStateChanged =
            this::onFuseboxStateChanged;

    private boolean mUrlHasFocus;
    private boolean mVerboseStatusSpaceAvailable;
    private boolean mPageIsPaintPreview;
    private boolean mPageIsOffline;
    private boolean mShowStatusIconWhenUrlFocused;
    private boolean mIsSecurityViewShown;
    private int mUrlMinWidth;
    private int mSeparatorMinWidth;
    private int mVerboseStatusTextMinWidth;
    private @ConnectionSecurityLevel int mPageSecurityLevel;
    private @BrandedColorScheme int mBrandedColorScheme = BrandedColorScheme.APP_DEFAULT;
    private @DrawableRes int mSecurityIconRes;
    private @ColorRes int mSecurityIconTintRes;
    private @StringRes int mSecurityIconDescriptionRes;
    private @ColorRes int mNavigationIconTintRes;
    private boolean mUrlBarTextIsSearch = true;
    private boolean mIsStoreIconShowing;
    private float mUrlFocusPercent;
    private @Nullable CookieControlsBridge mCookieControlsBridge;
    private @Nullable SearchEngineUtils mSearchEngineUtils;
    private @Nullable StatusIconResource mSearchEngineIcon;
    private @Nullable NullableObservableSupplier<SiteSearchData> mSiteSearchDataSupplier;
    private @Nullable OnClickListener mOnStatusIconNavigateBackButtonPress;
    private int mLastTabId;
    private boolean mCurrentTabCrashed;
    private Drawable mDefaultStatusBackground;
    private Drawable mDefaultStatusBackgroundIncognito;
    private Drawable mVerboseStatusBackground;
    private Drawable mVerboseStatusBackgroundIncognito;
    private boolean mShowStatusIconForSecureOrigins;

    /**
     * @param model The {@link PropertyModel} for this mediator.
     * @param context The {@link Context} for this Status component.
     * @param isTablet Whether the current device is a tablet.
     * @param locationBarDataProvider Provides data to the location bar.
     * @param permissionDialogController Controls showing permission dialogs.
     * @param templateUrlServiceSupplier Supplies the {@link TemplateUrlService}.
     * @param profileSupplier Supplies the current {@link Profile}.
     * @param pageInfoIphController Manages when an IPH bubble for PageInfo is shown.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param fuseboxStateSupplier Notifies about the state of the fusebox.
     * @param onPlusButtonClicked Toggle the fusebox attachments menu when plus button used.
     */
    public StatusMediator(
            PropertyModel model,
            Context context,
            boolean isTablet,
            LocationBarDataProvider locationBarDataProvider,
            PermissionDialogController permissionDialogController,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            MonotonicObservableSupplier<Profile> profileSupplier,
            PageInfoIphController pageInfoIphController,
            WindowAndroid windowAndroid,
            PageInfoAction pageInfoAction,
            NonNullObservableSupplier<Integer> fuseboxStateSupplier,
            Runnable onPlusButtonClicked) {
        initBackgroundDrawables(context);
        mModel = model;
        mModel.set(StatusProperties.USE_WIDE_STATUS_ICON, false);
        mLocationBarDataProvider = locationBarDataProvider;
        mTemplateUrlServiceSupplier = templateUrlServiceSupplier;
        mShowStatusIconForSecureOrigins = !isPageInfoMovedToAppMenu();
        mTemplateUrlServiceSupplier.onAvailable(
                (templateUrlService) -> {
                    templateUrlService.addObserver(this);
                    updateLocationBarIcon(IconTransitionType.CROSSFADE);
                });

        mProfileSupplier = profileSupplier;
        mContext = context;
        mPageInfoIphController = pageInfoIphController;

        mIsTablet = isTablet;
        mShowStatusIconWhenUrlFocused = mIsTablet;
        mPageInfoAction = pageInfoAction;
        mModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, false);

        mFuseboxStateSupplier = fuseboxStateSupplier;
        mFuseboxOnPlusButtonClicked = v -> onPlusButtonClicked.run();
        mFuseboxStateSupplier.addSyncObserver(mOnFuseboxStateChanged);

        mPermissionStatusHandler =
                new PermissionStatusHandler(
                        context,
                        locationBarDataProvider,
                        permissionDialogController,
                        pageInfoIphController,
                        profileSupplier,
                        windowAndroid,
                        this,
                        mIconTaskHandler);

        mProfileSupplier.addSyncObserverAndPostIfNonNull(
                p -> {
                    if (mSearchEngineUtils != null) {
                        mSearchEngineUtils.removeIconObserver(this);
                    }
                    mSearchEngineUtils = SearchEngineUtils.getForProfile(p);
                    mSearchEngineUtils.addIconObserver(this);
                });

        updateColorTheme();
        setStatusIconShown(/* show= */ true);
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
        updateStatusViewMinWidth();
    }

    public void destroy() {
        if (mSearchEngineUtils != null) {
            mSearchEngineUtils.removeIconObserver(this);
            mSearchEngineUtils = null;
        }

        mPermissionStatusHandler.destroy();
        mStoreIconHandler.removeCallbacksAndMessages(null);
        mIconTaskHandler.removeCallbacksAndMessages(null);

        var templateUrlService = mTemplateUrlServiceSupplier.get();
        if (templateUrlService != null) {
            templateUrlService.removeObserver(this);
        }
        if (mCookieControlsBridge != null) {
            mCookieControlsBridge.destroy();
            mCookieControlsBridge = null;
        }
        if (mFuseboxStateSupplier != null) {
            mFuseboxStateSupplier.removeObserver(mOnFuseboxStateChanged);
        }
    }

    /** Toggle animations of icon changes. */
    void setAnimationsEnabled(boolean enabled) {
        mModel.set(StatusProperties.ANIMATIONS_ENABLED, enabled);
    }

    /** Updates the icon, tint, and description of the security chip. */
    void updateSecurityIcon(
            @DrawableRes int securityIcon, @ColorRes int tintList, @StringRes int desc) {
        mSecurityIconRes = securityIcon;
        mSecurityIconTintRes = tintList;
        mSecurityIconDescriptionRes = desc;
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    @DrawableRes
    int getSecurityIconResource() {
        return mSecurityIconRes;
    }

    /**
     * Update the displayed page's security level and whether it's a paint preview or offline page.
     */
    void updateVerboseStatus(
            @ConnectionSecurityLevel int securityLevel,
            boolean pageIsOffline,
            boolean pageIsPaintPreview) {
        boolean didUpdate = false;
        if (mPageSecurityLevel != securityLevel) {
            mPageSecurityLevel = securityLevel;
            didUpdate = true;
        }

        if (mPageIsPaintPreview != pageIsPaintPreview) {
            mPageIsPaintPreview = pageIsPaintPreview;
            didUpdate = true;
        }

        if (mPageIsOffline != pageIsOffline) {
            mPageIsOffline = pageIsOffline;
            didUpdate = true;
        }

        if (didUpdate) {
            updateVerboseStatusTextVisibility();
            updateLocationBarIcon(IconTransitionType.CROSSFADE);
            updateColorTheme();
            updateStatusViewVisibility();
        }
    }

    void setShowStatusIconForSecureOrigins(boolean showStatusIconForSecureOrigins) {
        // Don't allow the update if the page is moved to the app menu as we don't show secure
        // origins then.
        if (isPageInfoMovedToAppMenu()) return;

        mShowStatusIconForSecureOrigins = showStatusIconForSecureOrigins;
        updateStatusViewVisibility();
    }

    /** Specify minimum width of the separator field. */
    void setSeparatorFieldMinWidth(int width) {
        mSeparatorMinWidth = width;
    }

    /** Specify whether status icon should be shown when URL is focused. */
    @VisibleForTesting
    void setShowIconsWhenUrlFocused(boolean showIconWhenFocused) {
        if (mShowStatusIconWhenUrlFocused == showIconWhenFocused) return;
        mShowStatusIconWhenUrlFocused = showIconWhenFocused;
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    /** Update unfocused location bar width to determine shape and content of the Status view. */
    void setUnfocusedLocationBarWidth(int width) {
        // This unfocused width is used rather than observing #onMeasure() to avoid showing the
        // verbose status when the animation to unfocus the URL bar has finished. There is a call to
        // LocationBarLayout#onMeasure() after the URL focus animation has finished and before the
        // location bar has received its updated width layout param.
        int computedSpace = width - mUrlMinWidth - mSeparatorMinWidth;
        boolean hasSpaceForStatus = width >= mVerboseStatusTextMinWidth;

        if (hasSpaceForStatus) {
            mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_WIDTH, computedSpace);
        }

        if (hasSpaceForStatus != mVerboseStatusSpaceAvailable) {
            mVerboseStatusSpaceAvailable = hasSpaceForStatus;
            updateVerboseStatusTextVisibility();
        }
    }

    /** Report URL focus change. */
    void setUrlHasFocus(boolean urlHasFocus) {
        if (mUrlHasFocus == urlHasFocus) return;

        mUrlHasFocus = urlHasFocus;
        updateVerboseStatusTextVisibility();
        updateStatusVisibility();
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
        updateStatusViewVisibility();
        updateStatusViewMinWidth();

        // Set the default match to be a search on an unfocus event to avoid the globe sticking
        // around for subsequent focus events.
        if (!mUrlHasFocus) updateLocationBarIconForDefaultMatchCategory(true);
    }

    private void updateStatusViewMinWidth() {
        // Don't use isNtpVisible() here -- it reports NTP visibility when navigation is already
        // underway, making the onUrlChanged fail to detect the user is navigating out of the NTP.
        var url = mLocationBarDataProvider.getCurrentGurl();
        boolean isRegularNtpUrl =
                url != null
                        && UrlUtilities.isNtpUrl(url)
                        && !mLocationBarDataProvider.isIncognitoBranded();

        mModel.set(StatusProperties.USE_WIDE_STATUS_ICON, mUrlHasFocus || isRegularNtpUrl);
    }

    void setStatusIconShown(boolean show) {
        applyStatusIconAndTooltipProperties(
                show, mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE));
    }

    public void setUseSmallWidget(boolean useSmallWidget) {
        mModel.set(StatusProperties.USE_SMALL_WIDGET, useSmallWidget);
    }

    void updateStatusVisibility() {
        // This logic doesn't apply to tablets.
        if (mIsTablet) return;

        setShowIconsWhenUrlFocused(true);
        setStatusIconShown(true);
    }

    /**
     * Set the url focus change percent.
     *
     * @param percent The current focus percent.
     */
    void setUrlFocusChangePercent(float percent) {
        // On tablets, the status icon should always be shown so the following logic doesn't apply.
        assert !mIsTablet : "This logic shouldn't be called on tablets";

        boolean couldAffectIcon =
                (mUrlFocusPercent == 0.0f && percent > 0.0f)
                        || (percent == 0.0f && mUrlFocusPercent > 0.0f);
        mUrlFocusPercent = percent;
        updateStatusVisibility();
        mModel.set(StatusProperties.STATUS_ICON_ALPHA, 1.0f);

        if (couldAffectIcon) {
            updateLocationBarIcon(IconTransitionType.CROSSFADE);
        }
    }

    /** Specify minimum width of an URL field. */
    void setUrlMinWidth(int width) {
        mUrlMinWidth = width;
    }

    /** Set the {@link BrandedColorScheme}. */
    void setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
        if (mBrandedColorScheme != brandedColorScheme) {
            mBrandedColorScheme = brandedColorScheme;
            updateColorTheme();
        }
    }

    /** Specify minimum width of the verbose status text field. */
    void setVerboseStatusTextMinWidth(int width) {
        mVerboseStatusTextMinWidth = width;
    }

    /** Update visibility of the verbose status text field. */
    private void updateVerboseStatusTextVisibility() {
        int statusText = 0;

        if (mPageIsPaintPreview) {
            statusText = R.string.location_bar_paint_preview_page_status;
        } else if (mPageIsOffline) {
            statusText = R.string.location_bar_verbose_status_offline;
        }

        // Decide whether presenting verbose status text makes sense.
        boolean newVisibility =
                shouldShowVerboseStatusText()
                        && mVerboseStatusSpaceAvailable
                        && !mUrlHasFocus
                        && (statusText != 0);

        // Update status content only if it is visible. Note: PropertyModel will help us avoid
        // duplicate updates with the same value.
        if (newVisibility) {
            mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES, statusText);
        }
        mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE, newVisibility);

        applyStatusIconAndTooltipProperties(
                mModel.get(StatusProperties.SHOW_STATUS_ICON), newVisibility);
    }

    /** Update color theme for all status components. */
    private void updateColorTheme() {
        final @ColorInt int separatorColor =
                OmniboxResourceProvider.getStatusSeparatorColor(mContext, mBrandedColorScheme);
        mModel.set(StatusProperties.SEPARATOR_COLOR, separatorColor);
        mNavigationIconTintRes = ThemeUtils.getThemedToolbarIconTintRes(mBrandedColorScheme);

        final @ColorInt int textColor = getTextColor();
        if (textColor != 0) {
            mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_COLOR, textColor);
        }

        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    private @ColorInt int getTextColor() {
        if (mPageIsPaintPreview) {
            return OmniboxResourceProvider.getStatusPreviewTextColor(mContext, mBrandedColorScheme);
        }
        if (mPageIsOffline) {
            return OmniboxResourceProvider.getStatusOfflineTextColor(mContext, mBrandedColorScheme);
        }
        return 0;
    }

    /** Reports whether security icon is shown. */
    @VisibleForTesting
    boolean isSecurityViewShown() {
        return mIsSecurityViewShown;
    }

    /** Compute verbose status text for the current page. */
    private boolean shouldShowVerboseStatusText() {
        return mPageIsOffline || mPageIsPaintPreview;
    }

    /**
     * Returns whether the NewTabPage is currently shown to the user.
     *
     * <p>Caution: returns true even if the user is currently navigating out of the NTP (Current URL
     * no longer pointing to NTP, but the navigation not yet completed).
     */
    private boolean isNtpVisible() {
        return mLocationBarDataProvider.getNewTabPageDelegate() != null
                && mLocationBarDataProvider.getNewTabPageDelegate().isCurrentlyVisible();
    }

    /**
     * Returns whether the Incognito NewTabPage is currently shown to the user.
     *
     * <p>Caution: returns true even if the user is currently navigating out of the NTP (Current URL
     * no longer pointing to NTP, but the navigation not yet completed).
     */
    private boolean isIncognitoNtpVisible() {
        return mLocationBarDataProvider.getNewTabPageDelegate() != null
                && mLocationBarDataProvider
                        .getNewTabPageDelegate()
                        .isIncognitoNewTabPageCurrentlyVisible();
    }

    @Override
    public void showPermissionIcon(PermissionIconResource icon) {
        mModel.set(StatusProperties.STATUS_ICON_RESOURCE, icon);
        mModel.set(StatusProperties.STATUS_ICON_DESCRIPTION_RES, icon.getContentDescriptionRes());
        mModel.set(StatusProperties.STATUS_CLICK_LISTENER, this::onClickOpenPageInfo);

        updateStatusViewVisibility();
    }

    /** Update selection of icon presented on the location bar. */
    @Override
    public void updateLocationBarIcon(@IconTransitionType int transitionType) {
        // Reset the store icon status.
        mIsStoreIconShowing = false;
        mIsSecurityViewShown = false;

        @DrawableRes int iconRes = 0;
        @ColorRes int tintRes = 0;
        @StringRes int toastRes = 0;
        @StringRes int descRes = Resources.ID_NULL;
        @StringRes int doubleTapDescriptionRes = R.string.accessibility_toolbar_view_site_info;
        OnClickListener clickListener = null;

        if (mFuseboxStateSupplier.get() == FuseboxState.COMPACT) {
            mPermissionStatusHandler.reset(/* shouldDismissNativePrompt= */ false);
            tintRes = mNavigationIconTintRes;
            iconRes = R.drawable.ic_add_round_20dp_with_inset;
            clickListener = mFuseboxOnPlusButtonClicked;
            descRes = R.string.accessibility_omnibox_open_context_popup;
            doubleTapDescriptionRes = Resources.ID_NULL;
        } else if (isContextualTasksFusebox()) {
            mPermissionStatusHandler.reset(/* shouldDismissNativePrompt= */ false);
            // No icon shown for Fusebox.
        } else if (maybeUpdateStatusIconForSearchEngineIcon()) {
            mPermissionStatusHandler.reset(/* shouldDismissNativePrompt= */ true);
            // No need to proceed further if we've already updated it for the search engine icon.
            return;
        } else if (isHubSearch()) {
            mPermissionStatusHandler.reset(/* shouldDismissNativePrompt= */ false);
            updateStatusViewVisibility();
            // Show the status icon primarily for incognito since it is defaulted off there.
            setStatusIconShown(/* show= */ true);
            iconRes = R.drawable.ic_arrow_back_24dp;
            tintRes = ThemeUtils.getThemedToolbarIconTintRes(mBrandedColorScheme);
            doubleTapDescriptionRes = R.string.accessibility_toolbar_exit_hub_search;
            applyStatusIconAndTooltipProperties(
                    mModel.get(StatusProperties.SHOW_STATUS_ICON),
                    mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE));
            clickListener = mOnStatusIconNavigateBackButtonPress;
        } else if (mUrlHasFocus) {
            mPermissionStatusHandler.reset(/* shouldDismissNativePrompt= */ true);
            if (mShowStatusIconWhenUrlFocused) {
                iconRes =
                        mUrlBarTextIsSearch
                                ? R.drawable.ic_suggestion_magnifier
                                : R.drawable.ic_globe_24dp;
                tintRes = mNavigationIconTintRes;
            }
        } else if (mPermissionStatusHandler.isClapperQuietIconShowing()) {
            return;
        } else if (mSecurityIconRes != 0) {
            if (mPageSecurityLevel == ConnectionSecurityLevel.SECURE
                    && (isPageInfoMovedToAppMenu() || !mShowStatusIconForSecureOrigins)) {
                mIsSecurityViewShown = false;
            } else {
                mIsSecurityViewShown = true;
                iconRes = mSecurityIconRes;
                tintRes = mSecurityIconTintRes;
                toastRes = R.string.menu_page_info;
                clickListener = this::onClickOpenPageInfo;
            }
        }

        // If the icon is missing, fallback to the info icon.
        StatusIconResource statusIcon =
                iconRes == 0 ? null : new StatusIconResource(iconRes, tintRes);
        if (statusIcon != null) {
            statusIcon.setTransitionType(transitionType);
        }

        // TODO(https://crbug.com/503744512): Description res decision should be integrated into
        //  this method's if/else statements.
        if (descRes == Resources.ID_NULL) {
            descRes = getAccessibilityDescriptionRes();
        }

        mModel.set(StatusProperties.STATUS_ICON_DESCRIPTION_RES, descRes);
        mModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIcon);
        mModel.set(StatusProperties.STATUS_ACCESSIBILITY_TOAST_RES, toastRes);
        mModel.set(
                StatusProperties.STATUS_ACCESSIBILITY_DOUBLE_TAP_DESCRIPTION_RES,
                doubleTapDescriptionRes);
        mModel.set(StatusProperties.STATUS_CLICK_LISTENER, clickListener);

        updateStatusViewVisibility();
    }

    void onFuseboxStateChanged(int state) {
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    /** Returns true if the security icon has been set for the search engine icon. */
    @VisibleForTesting
    boolean maybeUpdateStatusIconForSearchEngineIcon() {
        // Show the logo unfocused if we're on the NTP.
        if (!shouldDisplaySearchEngineIcon()) return false;

        mModel.set(
                StatusProperties.STATUS_ICON_RESOURCE, getStatusIconResourceForSearchEngineIcon());
        mModel.set(StatusProperties.STATUS_CLICK_LISTENER, null);
        updateStatusViewVisibility();
        return true;
    }

    /**
     * Returns whether the search engine icon should be displayed in the current context. This is
     * independent from alpha/visibility.
     */
    boolean shouldDisplaySearchEngineIcon() {
        if (isHubSearch() || isContextualTasksFusebox()) {
            return false;
        }

        if (mUrlHasFocus && mShowStatusIconWhenUrlFocused) {
            return true;
        }

        return (isNtpVisible() || isIncognitoNtpVisible()) && mProfileSupplier.get() != null;
    }

    /** Returns status icon resource for the user-selected default search engine. */
    private StatusIconResource getStatusIconResourceForSearchEngineIcon() {
        StatusIconResource extensionIcon = getStatusIconResourceForExtensionSuppliedDse();
        if (extensionIcon != null) return extensionIcon;

        if (mSiteSearchDataSupplier != null && mSiteSearchDataSupplier.get() != null) {
            return new StatusIconResource(
                    R.drawable.ic_suggestion_magnifier,
                    ThemeUtils.getThemedToolbarIconTintRes(mBrandedColorScheme));
        }

        // If the current url text is a valid url, then swap the dse icon for a globe.
        if (!mUrlBarTextIsSearch) {
            return new StatusIconResource(
                    R.drawable.ic_globe_24dp,
                    ThemeUtils.getThemedToolbarIconTintRes(mBrandedColorScheme));
        }

        if (mSearchEngineIcon == null) {
            return new StatusIconResource(
                    R.drawable.ic_search_24dp,
                    ThemeUtils.getThemedToolbarIconTintRes(mBrandedColorScheme));
        }

        return mSearchEngineIcon;
    }

    private @Nullable StatusIconResource getStatusIconResourceForExtensionSuppliedDse() {
        if (mSiteSearchDataSupplier == null) return null;

        SiteSearchData siteSearchData = mSiteSearchDataSupplier.get();
        TemplateUrlService turlService = mTemplateUrlServiceSupplier.get();
        if (siteSearchData == null || turlService == null) return null;

        Profile profile = mProfileSupplier.get();
        if (profile == null) return null;

        TemplateUrl templateUrl = turlService.getTemplateUrlForKeyword(siteSearchData.keyword);
        if (templateUrl == null || templateUrl.getProvidingExtensionId() == null) return null;

        Bitmap customIcon =
                ExtensionUi.getExtensionOmniboxIcon(profile, templateUrl.getProvidingExtensionId());
        if (customIcon == null) return null;

        return new StatusIconResource("site_search_icon", customIcon, 0);
    }

    /** Return the resource id for the accessibility description or 0 if none apply. */
    private int getAccessibilityDescriptionRes() {
        if (isHubSearch()) {
            return R.string.hub_search_status_view_back_button_icon_description;
        }

        if (mUrlHasFocus && !mLocationBarDataProvider.isIncognitoBranded()) {
            return 0;
        }
        return (mSecurityIconRes != 0) ? mSecurityIconDescriptionRes : 0;
    }

    /**
     * Informs StatusMediator that the default match may have changed categories, updating the
     * status icon if it has.
     */
    /* package */ void updateLocationBarIconForDefaultMatchCategory(boolean defaultMatchIsSearch) {
        if (defaultMatchIsSearch != mUrlBarTextIsSearch) {
            mUrlBarTextIsSearch = defaultMatchIsSearch;
            updateLocationBarIcon(IconTransitionType.CROSSFADE);
        }
    }

    // CookieControlsObserver interface.
    @Override
    public void onHighlightCookieControl(boolean shouldHighlight) {
        if (shouldHighlight) {
            animateCookieControlsIcon(
                    () -> {
                        mPageInfoIphController.showCookieControlsIph(
                                mPermissionStatusHandler.getIphTimeoutMs(),
                                R.string.cookie_controls_iph_message);
                    });
        }
    }

    private void animateCookieControlsIcon(Runnable onAnimationFinished) {
        // Check if the web content is valid before attempting to animate.
        Tab tab = mLocationBarDataProvider.getTab();
        if (tab == null || tab.getWebContents() == null) {
            return;
        }
        resetCustomIconsStatus();

        boolean isIncognitoBranded = mLocationBarDataProvider.isIncognitoBranded();
        Drawable eyeCrossedIcon =
                SettingsUtils.getTintedIcon(
                        mContext,
                        R.drawable.ic_eye_crossed,
                        isIncognitoBranded
                                ? R.color.default_icon_color_blue_light
                                : R.color.default_icon_color_accent1_tint_list);

        PermissionIconResource permissionIconResource =
                new PermissionIconResource(
                        eyeCrossedIcon, isIncognitoBranded, COOKIE_CONTROLS_ICON);
        permissionIconResource.setTransitionType(IconTransitionType.ROTATE);
        permissionIconResource.setAnimationFinishedCallback(
                () -> {
                    if (mCookieControlsBridge != null) {
                        mCookieControlsBridge.onEntryPointAnimated();
                    }
                    onAnimationFinished.run();
                });

        // Set the timer to switch the icon back afterwards.
        mIconTaskHandler.removeCallbacksAndMessages(null);
        mModel.set(StatusProperties.STATUS_ICON_RESOURCE, permissionIconResource);
        mModel.set(StatusProperties.STATUS_CLICK_LISTENER, this::onClickOpenPageInfo);
        mIconTaskHandler.postDelayed(
                () -> updateLocationBarIcon(IconTransitionType.ROTATE),
                PermissionStatusHandler.PERMISSION_ICON_DEFAULT_DISPLAY_TIMEOUT_MS);
    }

    // Reset all customized icons' status to avoid different icons' conflicts.
    @VisibleForTesting
    @Override
    public void resetCustomIconsStatus() {
        mPermissionStatusHandler.reset(/* shouldDismissNativePrompt= */ true);
        resetEmbeddedIconHandlers();

        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    private void openPageInfo(Tab tab) {
        mPageInfoAction.show(tab, getPageInfoHighlight());
        mPermissionStatusHandler.onPageInfoOpened();
        resetEmbeddedIconHandlers();
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    private void resetEmbeddedIconHandlers() {
        mStoreIconHandler.removeCallbacksAndMessages(null);
        mIconTaskHandler.removeCallbacksAndMessages(null);
        mIsStoreIconShowing = false;
    }

    /**
     * Returns {@link ChromePageInfoHighlight} which provides the PageInfo highlight row info when
     * user clicks the omnibox icon.
     */
    ChromePageInfoHighlight getPageInfoHighlight() {
        ChromePageInfoHighlight highlight = mPermissionStatusHandler.getPageInfoHighlight();
        if (highlight != null) {
            return highlight;
        } else if (mIsStoreIconShowing) {
            return ChromePageInfoHighlight.forStoreInfo(/* highlightStoreInfo= */ true);
        } else {
            return ChromePageInfoHighlight.noHighlight();
        }
    }

    @Override
    public void onTemplateURLServiceChanged() {
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    @Override
    public void onSearchEngineIconChanged(@Nullable StatusIconResource newIcon) {
        mSearchEngineIcon = newIcon;
        maybeUpdateStatusIconForSearchEngineIcon();
    }

    void setSiteSearchDataSupplier(@Nullable NullableObservableSupplier<SiteSearchData> supplier) {
        if (mSiteSearchDataSupplier != null) {
            mSiteSearchDataSupplier.removeObserver(mSiteSearchDataObserver);
        }
        mSiteSearchDataSupplier = supplier;
        if (mSiteSearchDataSupplier != null) {
            mSiteSearchDataSupplier.addSyncObserverAndCallIfNonNull(mSiteSearchDataObserver);
        }
    }

    private void onSiteSearchDataChanged(@Nullable SiteSearchData siteSearchData) {
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    void setTranslationX(float translationX) {
        mModel.set(StatusProperties.TRANSLATION_X, translationX);
    }

    void setTooltipText(@StringRes int tooltipTextResId) {
        applyStatusIconAndTooltipProperties(
                mModel.get(StatusProperties.SHOW_STATUS_ICON),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE));
    }

    void setBackground() {
        applyStatusIconAndTooltipProperties(
                mModel.get(StatusProperties.SHOW_STATUS_ICON),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE));
    }

    public void onUrlChanged() {
        updateStatusViewMinWidth();
        Tab currentTab = mLocationBarDataProvider.getTab();
        Profile profile = mProfileSupplier.get();
        if (profile != null && currentTab != null) {
            WebContents webContents = currentTab.getWebContents();
            if (webContents != null) {
                BrowserContextHandle originalBrowserContext =
                        profile.isOffTheRecord() ? profile.getOriginalProfile() : null;
                if (mCookieControlsBridge == null) {
                    mCookieControlsBridge =
                            new CookieControlsBridge(
                                    this,
                                    webContents,
                                    originalBrowserContext,
                                    profile.isIncognitoBranded());
                } else if (mLastTabId != currentTab.getId() || mCurrentTabCrashed) {
                    mCookieControlsBridge.updateWebContents(
                            webContents, originalBrowserContext, profile.isIncognitoBranded());
                    mCurrentTabCrashed = false;
                }
            }
            mLastTabId = currentTab.getId();
        }
    }

    public void onTabCrashed() {
        mCurrentTabCrashed = true;
    }

    void setShowStatusView(boolean show) {
        mModel.set(StatusProperties.SHOW_STATUS_VIEW, show);
    }

    /**
     * @param listener The custom listener that will execute when the status view is clicked.
     */
    void setOnStatusIconNavigateBackButtonPress(OnClickListener listener) {
        mOnStatusIconNavigateBackButtonPress = listener;
    }

    private void applyStatusIconAndTooltipProperties(
            boolean showIcon, boolean verboseStatusTextVisible) {
        mModel.set(StatusProperties.SHOW_STATUS_ICON, showIcon);
        if ((showIcon || verboseStatusTextVisible) && !isHubSearch()) {
            Drawable background;
            if (mLocationBarDataProvider.isIncognitoBranded()) {
                background =
                        verboseStatusTextVisible
                                ? mVerboseStatusBackgroundIncognito
                                : mDefaultStatusBackgroundIncognito;
            } else {
                background =
                        verboseStatusTextVisible
                                ? mVerboseStatusBackground
                                : mDefaultStatusBackground;
            }
            mModel.set(StatusProperties.STATUS_VIEW_BACKGROUND, background);
            mModel.set(StatusProperties.STATUS_VIEW_TOOLTIP_TEXT, R.string.accessibility_menu_info);
        } else {
            mModel.set(StatusProperties.STATUS_VIEW_TOOLTIP_TEXT, Resources.ID_NULL);
            mModel.set(StatusProperties.STATUS_VIEW_BACKGROUND, null);
        }
    }

    private void initBackgroundDrawables(Context context) {
        Resources res = context.getResources();
        int verboseStatusViewHeight =
                res.getDimensionPixelSize(R.dimen.status_view_verbose_highlight_height);
        int verboseStatusViewWidth =
                res.getDimensionPixelSize(R.dimen.status_view_verbose_highlight_width);
        mVerboseStatusBackground =
                DrawableUtils.getSearchBoxIconBackground(
                        context,
                        /* isIncognito= */ false,
                        verboseStatusViewHeight,
                        verboseStatusViewWidth);
        mVerboseStatusBackgroundIncognito =
                DrawableUtils.getSearchBoxIconBackground(
                        context,
                        /* isIncognito= */ true,
                        verboseStatusViewHeight,
                        verboseStatusViewWidth);

        int size = res.getDimensionPixelSize(R.dimen.small_icon_background_size);
        mDefaultStatusBackground =
                DrawableUtils.getIconBackground(context, /* isIncognito= */ false, size, size);
        mDefaultStatusBackgroundIncognito =
                DrawableUtils.getIconBackground(context, /* isIncognito= */ true, size, size);
    }

    private void updateStatusViewVisibility() {
        if (isContextualTasksFusebox()) {
            setShowStatusView(false);
            return;
        }

        setShowStatusView(
                mUrlHasFocus
                        || isHubSearch()
                        || mShowStatusIconForSecureOrigins
                        || mIsSecurityViewShown
                        || hasStatusIconResource()
                        || shouldShowVerboseStatusText()
                        || mIsStoreIconShowing);
    }

    private boolean hasStatusIconResource() {
        return mModel.get(StatusProperties.STATUS_ICON_RESOURCE) != null;
    }

    private void onClickOpenPageInfo(View view) {
        if (!mLocationBarDataProvider.hasTab()
                || assumeNonNull(mLocationBarDataProvider.getTab()).getWebContents() == null) {
            return;
        }

        if (UrlUtilities.isNtpUrl(mLocationBarDataProvider.getCurrentGurl())) return;

        openPageInfo(mLocationBarDataProvider.getTab());
    }

    private boolean isHubSearch() {
        return mLocationBarDataProvider.getPageClassification(/* prefetch= */ false)
                == PageClassification.ANDROID_HUB_VALUE;
    }

    private boolean isContextualTasksFusebox() {
        return mLocationBarDataProvider.getPageClassification(/* prefetch= */ false)
                == PageClassification.CO_BROWSING_COMPOSEBOX_VALUE;
    }

    private static boolean isPageInfoMovedToAppMenu() {
        return ChromeFeatureList.sAndroidPageInfoAsAppMenuItem.isEnabled();
    }

    @Nullable CookieControlsBridge getCookieControlsBridgeForTesting() {
        return mCookieControlsBridge;
    }

    void setCookieControlsBridgeForTesting(CookieControlsBridge cookieControlsBridge) {
        mCookieControlsBridge = cookieControlsBridge;
    }

    public PermissionStatusHandler getPermissionStatusHandlerForTesting() {
        return mPermissionStatusHandler;
    }

    boolean isStoreIconShowingForTesting() {
        return mIsStoreIconShowing;
    }
}
