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
import androidx.annotation.DimenRes;
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
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils.SearchEngineIconObserver;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxFeatureUtils;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator.PageInfoAction;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.PermissionIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusView.IconTransitionType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.extensions.ExtensionUi;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.util.DrawableUtils;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput.SiteSearchData;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.ToolModeUtils;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Objects;

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
    private final Context mContext;
    private final LocationBarDataProvider mLocationBarDataProvider;
    private final PermissionStatusHandler mPermissionStatusHandler;
    private final Handler mIconTaskHandler = new Handler();
    private final PageInfoIphController mPageInfoIphController;
    private final PageInfoAction mPageInfoAction;
    private final NonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier;
    private final NonNullObservableSupplier<@FuseboxLayoutMode Integer> mFuseboxLayoutModeSupplier;
    private final OnClickListener mFuseboxOnPlusButtonClicked;
    private final NullableObservableSupplier<GURL> mExactMatchUrlSupplier;
    private final OmniboxImageSupplier mImageSupplier;
    private @Nullable Runnable mOnStatusViewHiddenForPageInfoRemoval;
    private final Callback<@Nullable SiteSearchData> mSiteSearchDataObserver =
            this::onSiteSearchDataChanged;
    private final Callback<@FuseboxState Integer> mOnFuseboxStateChanged =
            this::onFuseboxStateChanged;
    private final Callback<@Nullable GURL> mOnExactMatchUrlChanged = this::onExactMatchUrlChanged;
    private final Callback<@AutocompleteRequestType Integer> mOnAutocompleteRequestTypeChanged =
            this::onAutocompleteRequestTypeChanged;

    private boolean mUrlHasFocus;
    private boolean mVerboseStatusSpaceAvailable;
    private boolean mPageIsPaintPreview;
    private boolean mPageIsOffline;
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
    private @Nullable CookieControlsBridge mCookieControlsBridge;
    private @Nullable SearchEngineUtils mSearchEngineUtils;
    private @Nullable StatusIconResource mSearchEngineIcon;
    private @Nullable StatusIconResource mSiteSearchFavicon;
    private @Nullable NullableObservableSupplier<SiteSearchData> mSiteSearchDataSupplier;
    private @Nullable OnClickListener mOnStatusIconNavigateBackButtonPress;
    private @Nullable FuseboxSessionState mInputSessionState;
    private int mLastTabId;
    private boolean mCurrentTabCrashed;
    private Drawable mDefaultStatusBackground;
    private Drawable mDefaultStatusBackgroundIncognito;
    private Drawable mVerboseStatusBackground;
    private Drawable mVerboseStatusBackgroundIncognito;
    private boolean mShowStatusIconForSecureOrigins;
    private @Nullable GURL mExactMatchFetchedUrl;
    private @Nullable Drawable mExactMatchFavicon;
    private boolean mShowExactMatchGlobe;

    /**
     * @param model The {@link PropertyModel} for this mediator.
     * @param context The {@link Context} for this Status component.
     * @param locationBarDataProvider Provides data to the location bar.
     * @param permissionDialogController Controls showing permission dialogs.
     * @param templateUrlServiceSupplier Supplies the {@link TemplateUrlService}.
     * @param profileSupplier Supplies the current {@link Profile}.
     * @param pageInfoIphController Manages when an IPH bubble for PageInfo is shown.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param pageInfoAction Callback to display the page info UI surface.
     * @param fuseboxStateSupplier Notifies about the state of the fusebox.
     * @param onPlusButtonClicked Toggle the fusebox attachments menu when plus button used.
     * @param fuseboxLayoutModeSupplier Notifies about the layout mode of the fusebox.
     * @param exactMatchUrlSupplier Holds the url of an exact match, null otherwise.
     */
    public StatusMediator(
            PropertyModel model,
            Context context,
            LocationBarDataProvider locationBarDataProvider,
            PermissionDialogController permissionDialogController,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            MonotonicObservableSupplier<Profile> profileSupplier,
            PageInfoIphController pageInfoIphController,
            WindowAndroid windowAndroid,
            PageInfoAction pageInfoAction,
            NonNullObservableSupplier<Integer> fuseboxStateSupplier,
            NonNullObservableSupplier<Integer> fuseboxLayoutModeSupplier,
            Runnable onPlusButtonClicked,
            NullableObservableSupplier<GURL> exactMatchUrlSupplier) {
        mContext = context;
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
        mPageInfoIphController = pageInfoIphController;

        mPageInfoAction = pageInfoAction;
        mModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, false);

        mFuseboxStateSupplier = fuseboxStateSupplier;
        mFuseboxLayoutModeSupplier = fuseboxLayoutModeSupplier;
        mFuseboxOnPlusButtonClicked = v -> onPlusButtonClicked.run();
        mFuseboxStateSupplier.addSyncObserver(mOnFuseboxStateChanged);

        mImageSupplier = new OmniboxImageSupplier(context);
        mExactMatchUrlSupplier = exactMatchUrlSupplier;
        mExactMatchUrlSupplier.addSyncObserver(mOnExactMatchUrlChanged);

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
                    mImageSupplier.setProfile(p);
                    updateLocationBarIcon(IconTransitionType.CROSSFADE);
                });

        updateColorTheme();
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
        updateStatusViewMinWidth();
    }

    public void destroy() {
        if (mSearchEngineUtils != null) {
            mSearchEngineUtils.removeIconObserver(this);
            mSearchEngineUtils = null;
        }

        mPermissionStatusHandler.destroy();
        mIconTaskHandler.removeCallbacksAndMessages(null);

        var templateUrlService = mTemplateUrlServiceSupplier.get();
        if (templateUrlService != null) {
            templateUrlService.removeObserver(this);
        }
        if (mCookieControlsBridge != null) {
            mCookieControlsBridge.destroy();
            mCookieControlsBridge = null;
        }
        mFuseboxStateSupplier.removeObserver(mOnFuseboxStateChanged);
        mExactMatchUrlSupplier.removeObserver(mOnExactMatchUrlChanged);
        mImageSupplier.destroy();
    }

    /**
     * Sets the callback to be executed when the status view is hidden due to the Page Info removal.
     *
     * @param runnable The callback to run.
     */
    void setOnStatusViewHiddenForPageInfoRemoval(Runnable runnable) {
        mOnStatusViewHiddenForPageInfoRemoval = runnable;
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

    void beginInput(FuseboxSessionState sessionState) {
        if (mUrlHasFocus) return;

        mUrlHasFocus = true;
        setFuseboxSessionState(sessionState);
        updateVerboseStatusTextVisibility();
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
        updateStatusViewVisibility();
        updateStatusViewMinWidth();

        @DimenRes
        int cornerRes =
                OmniboxFeatures.sExactMatchFavicons.isEnabled()
                        ? R.dimen.omnibox_small_icon_rounding_radius
                        : R.dimen.omnibox_search_engine_logo_composed_half_size;
        mModel.set(StatusProperties.STATUS_ICON_CORNER_RADIUS, cornerRes);
    }

    void endInput() {
        if (!mUrlHasFocus) return;

        mUrlHasFocus = false;
        setFuseboxSessionState(null);
        updateVerboseStatusTextVisibility();
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
        updateStatusViewVisibility();
        updateStatusViewMinWidth();

        @DimenRes int cornerRes = R.dimen.omnibox_search_engine_logo_composed_half_size;
        mModel.set(StatusProperties.STATUS_ICON_CORNER_RADIUS, cornerRes);
    }

    private void setFuseboxSessionState(@Nullable FuseboxSessionState sessionState) {
        if (sessionState == mInputSessionState) return;

        if (mInputSessionState != null) {
            setSiteSearchDataSupplier(null);
            mInputSessionState
                    .getAutocompleteInput()
                    .getRequestTypeSupplier()
                    .removeObserver(mOnAutocompleteRequestTypeChanged);
        }

        mInputSessionState = sessionState;

        if (mInputSessionState != null) {
            setSiteSearchDataSupplier(
                    mInputSessionState.getAutocompleteInput().getSiteSearchDataSupplier());
            mInputSessionState
                    .getAutocompleteInput()
                    .getRequestTypeSupplier()
                    .addSyncObserver(mOnAutocompleteRequestTypeChanged);
        }
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

    public void setUseSmallWidget(boolean useSmallWidget) {
        mModel.set(StatusProperties.USE_SMALL_WIDGET, useSmallWidget);
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

        applyStatusIconAndTooltipProperties(newVisibility);
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

    private boolean shouldShowNtpPlusButton() {
        Profile profile = mProfileSupplier.get();
        TemplateUrlService templateUrlService = mTemplateUrlServiceSupplier.get();
        return isNtpVisible()
                && !mUrlHasFocus
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                && FuseboxFeatureUtils.shouldShowNtpPlusButton(
                        mContext, profile, templateUrlService);
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
        mIsSecurityViewShown = false;

        @DrawableRes int iconRes = Resources.ID_NULL;
        @ColorRes int tintRes = Resources.ID_NULL;
        @StringRes int toastRes = Resources.ID_NULL;
        @StringRes int descRes = Resources.ID_NULL;
        @StringRes int doubleTapDescriptionRes = R.string.accessibility_toolbar_view_site_info;
        OnClickListener clickListener = null;
        Bitmap bitmap = null;
        Drawable customDrawable = null;

        boolean exactMatch = OmniboxFeatures.sExactMatchFavicons.isEnabled();
        @AutocompleteRequestType
        int requestType =
                mInputSessionState == null
                        ? AutocompleteRequestType.SEARCH
                        : mInputSessionState.getAutocompleteInput().getRequestType();

        if (isHubSearch()) {
            mPermissionStatusHandler.reset(/* shouldDismissNativePrompt= */ false);
            updateStatusViewVisibility();
            iconRes = R.drawable.ic_arrow_back_24dp;
            tintRes = ThemeUtils.getThemedToolbarIconTintRes(mBrandedColorScheme);
            doubleTapDescriptionRes = R.string.accessibility_toolbar_exit_hub_search;
            applyStatusIconAndTooltipProperties(
                    mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE));
            clickListener = mOnStatusIconNavigateBackButtonPress;
        } else if (exactMatch && mShowExactMatchGlobe) {
            mPermissionStatusHandler.reset(/* shouldDismissNativePrompt= */ false);
            iconRes = R.drawable.ic_globe_24dp;
            tintRes = mNavigationIconTintRes;
        } else if (exactMatch && mExactMatchFavicon != null) {
            mPermissionStatusHandler.reset(/* shouldDismissNativePrompt= */ false);
            customDrawable = mExactMatchFavicon;
        } else if (OmniboxCapabilities.isDesktopPlatform()
                && mInputSessionState != null
                && ToolModeUtils.isAimRequest(requestType)) {
            mPermissionStatusHandler.reset(/* shouldDismissNativePrompt= */ false);
            tintRes = mNavigationIconTintRes;
            iconRes = R.drawable.search_spark_black_24dp;
            descRes = R.string.accessibility_omnibox_open_context_popup;
            doubleTapDescriptionRes = Resources.ID_NULL;
        } else if (mFuseboxLayoutModeSupplier.get() == FuseboxLayoutMode.TOOLBAR
                && (mFuseboxStateSupplier.get() == FuseboxState.COMPACT
                        || shouldShowNtpPlusButton())) {
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
        } else if (mUrlHasFocus) {
            mPermissionStatusHandler.reset(/* shouldDismissNativePrompt= */ true);
            iconRes =
                    isUrlBarTextSearch()
                            ? R.drawable.ic_suggestion_magnifier
                            : R.drawable.ic_globe_24dp;
            tintRes = mNavigationIconTintRes;
        } else if (mPermissionStatusHandler.isClapperQuietIconShowing()) {
            return;
        } else if (mSecurityIconRes != Resources.ID_NULL) {
            if (mPageSecurityLevel == ConnectionSecurityLevel.SECURE
                    && (isPageInfoMovedToAppMenu() || !mShowStatusIconForSecureOrigins)) {
                mIsSecurityViewShown = false;
                if (mOnStatusViewHiddenForPageInfoRemoval != null) {
                    mOnStatusViewHiddenForPageInfoRemoval.run();
                }
            } else {
                mIsSecurityViewShown = true;
                iconRes = mSecurityIconRes;
                tintRes = mSecurityIconTintRes;
                if (isPageInfoMovedToAppMenu()) {
                    toastRes = Resources.ID_NULL;
                    clickListener = null;
                } else {
                    toastRes = R.string.menu_page_info;
                    clickListener = this::onClickOpenPageInfo;
                }
            }
        }

        StatusIconResource statusIcon = null;
        if (customDrawable != null) {
            statusIcon = new StatusIconResource(customDrawable);
        } else if (bitmap != null) {
            statusIcon = new StatusIconResource(/* iconIdentifier= */ null, bitmap, tintRes);
        } else if (iconRes != Resources.ID_NULL) {
            statusIcon = new StatusIconResource(iconRes, tintRes);
        }

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

    private void onFuseboxStateChanged(@FuseboxState int state) {
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    private void onAutocompleteRequestTypeChanged(@AutocompleteRequestType int type) {
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

        if (mUrlHasFocus) {
            return true;
        }

        return (isNtpVisible() || isIncognitoNtpVisible()) && mProfileSupplier.get() != null;
    }

    /** Returns status icon resource for the user-selected default search engine. */
    private StatusIconResource getStatusIconResourceForSearchEngineIcon() {
        StatusIconResource extensionIcon = getStatusIconResourceForExtensionSuppliedDse();
        if (extensionIcon != null) return extensionIcon;

        if (mSiteSearchDataSupplier != null && mSiteSearchDataSupplier.get() != null) {
            if (mSiteSearchFavicon != null) return mSiteSearchFavicon;
            return new StatusIconResource(
                    R.drawable.ic_suggestion_magnifier,
                    ThemeUtils.getThemedToolbarIconTintRes(mBrandedColorScheme));
        }

        // If the current url text is a valid url, then swap the dse icon for a globe.
        if (!isUrlBarTextSearch()) {
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
        if (isPageInfoMovedAndConnectionNotSecure()) {
            return 0;
        }
        return (mSecurityIconRes != 0) ? mSecurityIconDescriptionRes : 0;
    }

    private void onExactMatchUrlChanged(@Nullable GURL url) {
        if (!OmniboxFeatures.sExactMatchFavicons.isEnabled()) {
            if ((mExactMatchFetchedUrl == null) != (url == null)) {
                mExactMatchFetchedUrl = url;
                updateLocationBarIcon(IconTransitionType.CROSSFADE);
            }
            return;
        }

        mExactMatchFetchedUrl = url;
        if (url == null) {
            mExactMatchFavicon = null;
            mShowExactMatchGlobe = false;
            updateLocationBarIcon(IconTransitionType.CROSSFADE);
        } else {
            mImageSupplier.fetchFavicon(url, drawable -> onFaviconFetched(url, drawable));
        }
    }

    private void onFaviconFetched(GURL url, @Nullable Drawable favicon) {
        // If we're not the most recent fetch request, give up.
        if (!url.equals(mExactMatchFetchedUrl)) return;

        boolean useGlobe = favicon == null;

        if (mShowExactMatchGlobe && useGlobe) {
            return;
        }

        mExactMatchFavicon = favicon;
        mShowExactMatchGlobe = useGlobe;
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
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
        mIconTaskHandler.removeCallbacksAndMessages(null);
    }

    /**
     * Returns {@link ChromePageInfoHighlight} which provides the PageInfo highlight row info when
     * user clicks the omnibox icon.
     */
    ChromePageInfoHighlight getPageInfoHighlight() {
        ChromePageInfoHighlight highlight = mPermissionStatusHandler.getPageInfoHighlight();
        if (highlight != null) {
            return highlight;
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

    private void setSiteSearchDataSupplier(
            @Nullable NullableObservableSupplier<SiteSearchData> supplier) {
        if (mSiteSearchDataSupplier != null) {
            mSiteSearchDataSupplier.removeObserver(mSiteSearchDataObserver);
        }
        mSiteSearchDataSupplier = supplier;
        if (mSiteSearchDataSupplier != null) {
            mSiteSearchDataSupplier.addSyncObserverAndCallIfNonNull(mSiteSearchDataObserver);
        }
    }

    private void onSiteSearchDataChanged(@Nullable SiteSearchData siteSearchData) {
        mSiteSearchFavicon = null;
        updateLocationBarIcon(IconTransitionType.CROSSFADE);

        if (siteSearchData == null || mSearchEngineUtils == null) return;

        TemplateUrlService templateUrlService = mTemplateUrlServiceSupplier.get();
        if (templateUrlService == null) return;

        TemplateUrl templateUrl =
                templateUrlService.getTemplateUrlForKeyword(siteSearchData.keyword);
        if (templateUrl == null) return;

        mSearchEngineUtils.retrieveFavicon(
                templateUrl,
                icon -> {
                    // Only set the icon if the SiteSearchData has not changed. There might be edge
                    // cases where site search data changes between calls.
                    if (mSiteSearchDataSupplier != null
                            && Objects.equals(siteSearchData, mSiteSearchDataSupplier.get())) {
                        mSiteSearchFavicon = icon;
                        updateLocationBarIcon(IconTransitionType.CROSSFADE);
                    }
                });
    }

    void setTranslationX(float translationX) {
        mModel.set(StatusProperties.TRANSLATION_X, translationX);
    }

    void setTooltipText(@StringRes int tooltipTextResId) {
        applyStatusIconAndTooltipProperties(
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE));
    }

    void setBackground() {
        applyStatusIconAndTooltipProperties(
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
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
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

    private void applyStatusIconAndTooltipProperties(boolean verboseStatusTextVisible) {
        if (!isHubSearch()) {
            Drawable background;
            if (isPageInfoMovedAndConnectionNotSecure()) {
                background = null;
            } else if (mLocationBarDataProvider.isIncognitoBranded()) {
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
            mModel.set(
                    StatusProperties.STATUS_VIEW_TOOLTIP_TEXT,
                    isPageInfoMovedToAppMenu()
                            ? Resources.ID_NULL
                            : R.string.accessibility_menu_info);
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
                        || shouldShowVerboseStatusText());
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

    private boolean isUrlBarTextSearch() {
        return mExactMatchUrlSupplier.get() == null;
    }

    private boolean isPageInfoMovedToAppMenu() {
        return BrowserUiUtils.isPageInfoMovedToAppMenu(mContext);
    }

    private boolean isPageInfoMovedAndConnectionNotSecure() {
        return isPageInfoMovedToAppMenu() && mPageSecurityLevel != ConnectionSecurityLevel.SECURE;
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
}
