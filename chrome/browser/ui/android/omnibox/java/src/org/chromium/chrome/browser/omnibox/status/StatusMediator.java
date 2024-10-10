// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.PermissionIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusView.IconTransitionType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SiteSettingsUtil;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieBlocking3pcdStatus;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Contains the controller logic of the Status component. */
public class StatusMediator
        implements PermissionDialogController.Observer,
                TemplateUrlServiceObserver,
                MerchantTrustSignalsCoordinator.OmniboxIconController,
                CookieControlsObserver {
    private static final int PERMISSION_ICON_DEFAULT_DISPLAY_TIMEOUT_MS = 8500;
    public static final String PERMISSION_ICON_TIMEOUT_MS_PARAM = "PermissionIconTimeoutMs";

    static final String COOKIE_CONTROLS_ICON = "COOKIE_CONTROLS_ICON";

    private final PropertyModel mModel;
    private final OneshotSupplier<TemplateUrlService> mTemplateUrlServiceSupplier;
    private final Supplier<Profile> mProfileSupplier;
    private final Supplier<MerchantTrustSignalsCoordinator>
            mMerchantTrustSignalsCoordinatorSupplier;
    private boolean mUrlHasFocus;
    private boolean mVerboseStatusSpaceAvailable;
    private boolean mPageIsPaintPreview;
    private boolean mPageIsOffline;
    private boolean mShowStatusIconWhenUrlFocused;
    private boolean mIsSecurityViewShown;
    private boolean mIsTablet;

    private int mUrlMinWidth;
    private int mSeparatorMinWidth;
    private int mVerboseStatusTextMinWidth;

    private @ConnectionSecurityLevel int mPageSecurityLevel;

    private @BrandedColorScheme int mBrandedColorScheme = BrandedColorScheme.APP_DEFAULT;
    private @DrawableRes int mSecurityIconRes;
    private @ColorRes int mSecurityIconTintRes;
    private @StringRes int mSecurityIconDescriptionRes;
    private @ColorRes int mNavigationIconTintRes;

    private Context mContext;

    private LocationBarDataProvider mLocationBarDataProvider;
    private UrlBarEditingTextStateProvider mUrlBarEditingTextStateProvider;

    private final PermissionDialogController mPermissionDialogController;
    private final Handler mPermissionTaskHandler = new Handler();
    private final Handler mStoreIconHandler = new Handler();
    private @ContentSettingsType.EnumType int mLastPermission = ContentSettingsType.DEFAULT;
    private final PageInfoIPHController mPageInfoIPHController;
    private final WindowAndroid mWindowAndroid;

    private boolean mUrlBarTextIsSearch = true;

    private boolean mIsStoreIconShowing;

    private float mUrlFocusPercent;

    private int mPermissionIconDisplayTimeoutMs = PERMISSION_ICON_DEFAULT_DISPLAY_TIMEOUT_MS;

    private CookieControlsBridge mCookieControlsBridge;
    private boolean mCookieControlsVisible;
    private boolean mThirdPartyCookiesBlocked;
    private int mBlockingStatus3pcd;
    private int mLastTabId;
    private boolean mCurrentTabCrashed;

    /**
     * @param model The {@link PropertyModel} for this mediator.
     * @param context The {@link Context} for this Status component.
     * @param urlBarEditingTextStateProvider Provides url bar text state.
     * @param isTablet Whether the current device is a tablet.
     * @param locationBarDataProvider Provides data to the location bar.
     * @param permissionDialogController Controls showing permission dialogs.
     * @param templateUrlServiceSupplier Supplies the {@link TemplateUrlService}.
     * @param profileSupplier Supplies the current {@link Profile}.
     * @param pageInfoIPHController Manages when an IPH bubble for PageInfo is shown.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param merchantTrustSignalsCoordinatorSupplier Supplier of {@link
     *     MerchantTrustSignalsCoordinator}. Can be null if a store icon shouldn't be shown, such as
     *     when called from a search activity.
     */
    public StatusMediator(
            PropertyModel model,
            Context context,
            UrlBarEditingTextStateProvider urlBarEditingTextStateProvider,
            boolean isTablet,
            LocationBarDataProvider locationBarDataProvider,
            PermissionDialogController permissionDialogController,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            Supplier<Profile> profileSupplier,
            PageInfoIPHController pageInfoIPHController,
            WindowAndroid windowAndroid,
            @Nullable
                    Supplier<MerchantTrustSignalsCoordinator>
                            merchantTrustSignalsCoordinatorSupplier) {
        mModel = model;
        mLocationBarDataProvider = locationBarDataProvider;
        mTemplateUrlServiceSupplier = templateUrlServiceSupplier;
        mTemplateUrlServiceSupplier.onAvailable(
                (templateUrlService) -> {
                    templateUrlService.addObserver(this);
                    updateLocationBarIcon(IconTransitionType.CROSSFADE);
                });

        mProfileSupplier = profileSupplier;
        mContext = context;
        mUrlBarEditingTextStateProvider = urlBarEditingTextStateProvider;
        mPageInfoIPHController = pageInfoIPHController;
        mWindowAndroid = windowAndroid;
        mMerchantTrustSignalsCoordinatorSupplier = merchantTrustSignalsCoordinatorSupplier;

        mIsTablet = isTablet;
        mShowStatusIconWhenUrlFocused = mIsTablet;

        mPermissionDialogController = permissionDialogController;
        mPermissionDialogController.addObserver(this);

        updateColorTheme();
        setStatusIconShown(/* show= */ !mLocationBarDataProvider.isIncognitoBranded());
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    public void destroy() {
        mPermissionTaskHandler.removeCallbacksAndMessages(null);
        mPermissionDialogController.removeObserver(this);
        mStoreIconHandler.removeCallbacksAndMessages(null);
        if (mMerchantTrustSignalsCoordinatorSupplier != null
                && mMerchantTrustSignalsCoordinatorSupplier.get() != null) {
            mMerchantTrustSignalsCoordinatorSupplier.get().setOmniboxIconController(null);
        }

        if (mTemplateUrlServiceSupplier.hasValue()) {
            mTemplateUrlServiceSupplier.get().removeObserver(this);
        }
        if (mCookieControlsBridge != null) {
            mCookieControlsBridge.destroy();
            mCookieControlsBridge = null;
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
            updateVerbaseStatusTextVisibility();
            updateLocationBarIcon(IconTransitionType.CROSSFADE);
            updateColorTheme();
        }
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

    /**
     * Specify object to receive status click events.
     *
     * @param listener Specifies target object to receive events.
     */
    void setStatusClickListener(View.OnClickListener listener) {
        mModel.set(StatusProperties.STATUS_CLICK_LISTENER, listener);
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
            updateVerbaseStatusTextVisibility();
        }
    }

    /** Report URL focus change. */
    void setUrlHasFocus(boolean urlHasFocus) {
        if (mUrlHasFocus == urlHasFocus) return;

        mUrlHasFocus = urlHasFocus;
        updateVerbaseStatusTextVisibility();
        updateStatusVisibility();
        updateLocationBarIcon(IconTransitionType.CROSSFADE);

        // Set the default match to be a search on an unfocus event to avoid the globe sticking
        // around for subsequent focus events.
        if (!mUrlHasFocus) updateLocationBarIconForDefaultMatchCategory(true);
    }

    void setStatusIconShown(boolean show) {
        mModel.set(StatusProperties.SHOW_STATUS_ICON, show);
    }

    void setStatusIconAlpha(float alpha) {
        mModel.set(StatusProperties.STATUS_ICON_ALPHA, alpha);
    }

    void updateStatusVisibility() {
        // This logic doesn't apply to tablets.
        if (mIsTablet) return;

        boolean shouldShowLogo = !mLocationBarDataProvider.isIncognitoBranded();
        setShowIconsWhenUrlFocused(shouldShowLogo);
        if (!shouldShowLogo) return;

        if (mProfileSupplier.hasValue() && isNtpVisible()) {
            setStatusIconShown(shouldShowLogo && (mUrlHasFocus || mUrlFocusPercent > 0));
        } else {
            setStatusIconShown(true);
        }
    }

    /**
     * Sets the visibility of the status icon background.
     *
     * @param show True to make it visible.
     */
    void setStatusIconBackgroundVisibility(boolean show) {
        mModel.set(StatusProperties.SHOW_STATUS_ICON_BACKGROUND, show);
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

        // Only fade the animation on the new tab page.
        if (mProfileSupplier.hasValue() && isNtpVisible()) {
            setStatusIconAlpha(percent);
        } else {
            setStatusIconAlpha(1f);
        }

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
    private void updateVerbaseStatusTextVisibility() {
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

        // Update status content only if it is visible.
        // Note: PropertyModel will help us avoid duplicate updates with the
        // same value.
        if (newVisibility) {
            mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES, statusText);
        }

        mModel.set(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE, newVisibility);
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

    /** Get a CookieControlsBridge instance for testing purposes. */
    @VisibleForTesting
    CookieControlsBridge getCookieControlsBridge() {
        return mCookieControlsBridge;
    }

    /** Set a CookieControlsBridge instance for testing purposes. */
    @VisibleForTesting
    void setCookieControlsBridge(CookieControlsBridge cookieControlsBridge) {
        mCookieControlsBridge = cookieControlsBridge;
    }

    /** Compute verbose status text for the current page. */
    private boolean shouldShowVerboseStatusText() {
        return mPageIsOffline || mPageIsPaintPreview;
    }

    private boolean isNtpVisible() {
        return mLocationBarDataProvider.getNewTabPageDelegate().isCurrentlyVisible();
    }

    /**
     * Update selection of icon presented on the location bar.
     *
     * <ul>
     *   <li>Navigation button is:
     *       <ul>
     *         <li>shown only on large form factor devices (tablets and up)
     *         <li>shown only if URL is focused.
     *       </ul>
     *   <li>Security icon is:
     *       <ul>
     *         <li>shown only if specified,
     *         <li>not shown if URL is focused.
     *       </ul>
     * </ul>
     */
    void updateLocationBarIcon(@IconTransitionType int transitionType) {
        // Reset the last saved permission.
        mLastPermission = ContentSettingsType.DEFAULT;
        // Reset the store icon status.
        mIsStoreIconShowing = false;
        // Update the accessibility description before continuing since we need it either way.
        mModel.set(StatusProperties.STATUS_ICON_DESCRIPTION_RES, getAccessibilityDescriptionRes());

        // No need to proceed further if we've already updated it for the search engine icon.
        if (maybeUpdateStatusIconForSearchEngineIcon()) return;

        int icon = 0;
        int tint = 0;
        int toast = 0;

        mIsSecurityViewShown = false;
        if (mUrlHasFocus) {
            if (mShowStatusIconWhenUrlFocused) {
                icon =
                        mUrlBarTextIsSearch
                                ? R.drawable.ic_suggestion_magnifier
                                : R.drawable.ic_globe_24dp;
                tint = mNavigationIconTintRes;
            }
        } else if (mSecurityIconRes != 0) {
            mIsSecurityViewShown = true;
            icon = mSecurityIconRes;
            tint = mSecurityIconTintRes;
            toast = R.string.menu_page_info;
        }

        // If the icon is missing, fallback to the info icon.
        StatusIconResource statusIcon = icon == 0 ? null : new StatusIconResource(icon, tint);
        if (statusIcon != null) {
            statusIcon.setTransitionType(transitionType);
        }

        mModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIcon);
        mModel.set(StatusProperties.STATUS_ACCESSIBILITY_TOAST_RES, toast);
        mModel.set(
                StatusProperties.STATUS_ACCESSIBILITY_DOUBLE_TAP_DESCRIPTION_RES,
                R.string.accessibility_toolbar_view_site_info);
    }

    /**
     * @return True if the security icon has been set for the search engine icon.
     */
    @VisibleForTesting
    boolean maybeUpdateStatusIconForSearchEngineIcon() {
        // Show the logo unfocused if we're on the NTP.
        if (!shouldDisplaySearchEngineIcon()) return false;

        mModel.set(
                StatusProperties.STATUS_ICON_RESOURCE, getStatusIconResourceForSearchEngineIcon());
        return true;
    }

    /**
     * Returns whether the search engine icon should be displayed in the current context. This is
     * independent from alpha/visibility.
     */
    boolean shouldDisplaySearchEngineIcon() {
        if (mLocationBarDataProvider.isIncognitoBranded()) {
            return false;
        }

        if (mUrlHasFocus && mShowStatusIconWhenUrlFocused) {
            return true;
        }

        return (mUrlHasFocus || mUrlFocusPercent > 0)
                && isNtpVisible()
                && mProfileSupplier.hasValue();
    }

    /** Returns status icon resource for the user-selected default search engine. */
    private @NonNull StatusIconResource getStatusIconResourceForSearchEngineIcon() {
        // If the current url text is a valid url, then swap the dse icon for a globe.
        if (!mUrlBarTextIsSearch) {
            return SearchEngineUtils.getFallbackNavigationIcon(mBrandedColorScheme);
        }

        if (!mProfileSupplier.hasValue()) {
            return SearchEngineUtils.getFallbackSearchIcon(mBrandedColorScheme);
        }

        var profile = mProfileSupplier.get();
        return SearchEngineUtils.getForProfile(profile).getSearchEngineLogo(mBrandedColorScheme);
    }

    /** Return the resource id for the accessibility description or 0 if none apply. */
    private int getAccessibilityDescriptionRes() {
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

    @VisibleForTesting
    protected String resolveUrlBarTextWithAutocomplete(CharSequence urlBarText) {
        String currentAutocompleteText = mUrlBarEditingTextStateProvider.getTextWithAutocomplete();
        String urlTextWithAutocomplete;
        if (TextUtils.isEmpty(urlBarText)) {
            // TODO (crbug.com/1012870): This is to workaround the UrlBar text being empty but the
            // autocomplete text still pointing at the previous url's autocomplete text.
            urlTextWithAutocomplete = "";
        } else if (TextUtils.indexOf(currentAutocompleteText, urlBarText) > -1) {
            // TODO(crbug.com/40103581): This is to workaround the UrlBar text pointing to the
            // "current" url and the the autocomplete text pointing to the "previous" url.
            urlTextWithAutocomplete = currentAutocompleteText;
        } else {
            // If the above cases don't apply, then we should use the UrlBar text itself.
            urlTextWithAutocomplete = urlBarText.toString();
        }

        return urlTextWithAutocomplete;
    }

    public void onIncognitoStateChanged() {
        boolean incognitoBadgeVisible = mLocationBarDataProvider.isIncognitoBranded();
        mModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, incognitoBadgeVisible);
        mModel.set(StatusProperties.STATUS_ICON_RESOURCE, null);
        setStatusIconAlpha(1f);
        setStatusIconShown(false);
    }

    // PermissionDialogController.Observer interface
    @Override
    public void onDialogResult(
            WindowAndroid window,
            @ContentSettingsType.EnumType int[] permissions,
            @ContentSettingValues int result) {
        if (window != mWindowAndroid) {
            return;
        }
        @ContentSettingsType.EnumType
        int permission = SiteSettingsUtil.getHighestPriorityPermission(permissions);
        // The permission is not available in the settings page. Do not show an icon.
        if (permission == ContentSettingsType.DEFAULT) return;
        resetCustomIconsStatus();
        mLastPermission = permission;

        boolean isIncognitoBranded = mLocationBarDataProvider.isIncognitoBranded();
        Drawable permissionDrawable =
                ContentSettingsResources.getIconForOmnibox(
                        mContext, mLastPermission, result, isIncognitoBranded);
        PermissionIconResource permissionIconResource =
                new PermissionIconResource(permissionDrawable, isIncognitoBranded);
        permissionIconResource.setTransitionType(IconTransitionType.ROTATE);
        // We only want to notify the IPH controller after the icon transition is finished.
        // IPH is controlled by the FeatureEngagement system through finch with a field trial
        // testing configuration.
        permissionIconResource.setAnimationFinishedCallback(this::startIPH);
        // Set the timer to switch the icon back afterwards.
        mPermissionTaskHandler.removeCallbacksAndMessages(null);
        mModel.set(StatusProperties.STATUS_ICON_RESOURCE, permissionIconResource);
        Runnable finishIconAnimation = () -> updateLocationBarIcon(IconTransitionType.ROTATE);
        mPermissionTaskHandler.postDelayed(finishIconAnimation, mPermissionIconDisplayTimeoutMs);
    }

    // CookieControlsObserver interface
    @Override
    public void onHighlightCookieControl(boolean shouldHighlight) {
        if (shouldHighlight) {
            animateCookieControlsIcon(
                    () -> {
                        if (mBlockingStatus3pcd == CookieBlocking3pcdStatus.NOT_IN3PCD) {
                            mPageInfoIPHController.showCookieControlsIPH(
                                    getIPHTimeout(), R.string.cookie_controls_iph_message);
                        }
                    });
        }
    }

    @Override
    public void onStatusChanged(
            boolean controlsVisible,
            boolean protectionsOn,
            int enforcement,
            int blockingStatus,
            long expiration) {
        mCookieControlsVisible = controlsVisible;
        mThirdPartyCookiesBlocked = protectionsOn;
        mBlockingStatus3pcd = blockingStatus;
    }

    private void animateCookieControlsIcon(Runnable onAnimationFinished) {
        // Check if the web content is valid before attempting to animate.
        if (mLocationBarDataProvider.getTab().getWebContents() == null) {
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
        mPermissionTaskHandler.removeCallbacksAndMessages(null);
        mModel.set(StatusProperties.STATUS_ICON_RESOURCE, permissionIconResource);
        mPermissionTaskHandler.postDelayed(
                () -> updateLocationBarIcon(IconTransitionType.ROTATE),
                mPermissionIconDisplayTimeoutMs);
    }

    private void startIPH() {
        if (!mProfileSupplier.hasValue()) return;
        mPageInfoIPHController.onPermissionDialogShown(mProfileSupplier.get(), getIPHTimeout());
    }

    void setStoreIconController() {
        if (mMerchantTrustSignalsCoordinatorSupplier != null
                && mMerchantTrustSignalsCoordinatorSupplier.get() != null) {
            mMerchantTrustSignalsCoordinatorSupplier.get().setOmniboxIconController(this);
        }
    }

    // MerchantTrustSignalsCoordinator.OmniboxIconController interface
    @Override
    public void showStoreIcon(
            WindowAndroid window,
            String url,
            Drawable drawable,
            @StringRes int stringId,
            boolean canShowIph) {
        if ((window != mWindowAndroid)
                || !url.equals(mLocationBarDataProvider.getCurrentGurl().getSpec())
                || mLocationBarDataProvider.isOffTheRecord()) {
            return;
        }
        resetCustomIconsStatus();
        // Use {@link PermissionIconResource} instead of {@link StatusIconResource} to encapsulate
        // the icon with a circle background.
        StatusIconResource storeIconResource = new PermissionIconResource(drawable, false);
        storeIconResource.setTransitionType(IconTransitionType.ROTATE);
        storeIconResource.setAnimationFinishedCallback(
                () -> {
                    if (canShowIph) {
                        mPageInfoIPHController.showStoreIconIPH(getIPHTimeout(), stringId);
                    }
                });
        mModel.set(StatusProperties.STATUS_ICON_RESOURCE, storeIconResource);
        mStoreIconHandler.postDelayed(
                () -> {
                    updateLocationBarIcon(IconTransitionType.ROTATE);
                },
                mPermissionIconDisplayTimeoutMs);
        mIsStoreIconShowing = true;
    }

    // Reset all customized icons' status to avoid different icons' conflicts.
    @VisibleForTesting
    void resetCustomIconsStatus() {
        mPermissionTaskHandler.removeCallbacksAndMessages(null);
        mLastPermission = ContentSettingsType.DEFAULT;
        mStoreIconHandler.removeCallbacksAndMessages(null);
        mIsStoreIconShowing = false;
    }

    /**
     * @return A timeout for the IPH bubble. The bubble is shown after the permission icon animation
     *     finishes and should disappear when it animates out.
     */
    private int getIPHTimeout() {
        return mPermissionIconDisplayTimeoutMs - (2 * StatusView.ICON_ROTATION_DURATION_MS);
    }

    /** Notifies that the page info was opened. */
    void onPageInfoOpened() {
        resetCustomIconsStatus();
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    public int getLastPermission() {
        return mLastPermission;
    }

    boolean isStoreIconShowing() {
        return mIsStoreIconShowing;
    }

    /**
     * @return {@link ChromePageInfoHighlight} which provides the PageInfo highlight row info when
     *     user clicks the omnibox icon.
     */
    ChromePageInfoHighlight getPageInfoHighlight() {
        if (mLastPermission != PageInfoController.NO_HIGHLIGHTED_PERMISSION) {
            return ChromePageInfoHighlight.forPermission(mLastPermission);
        } else if (mIsStoreIconShowing) {
            return ChromePageInfoHighlight.forStoreInfo(true);
        } else {
            return ChromePageInfoHighlight.noHighlight();
        }
    }

    @Override
    public void onTemplateURLServiceChanged() {
        updateLocationBarIcon(IconTransitionType.CROSSFADE);
    }

    void setTranslationX(float translationX) {
        mModel.set(StatusProperties.TRANSLATION_X, translationX);
    }

    void setTooltipText(@StringRes int tooltipTextResId) {
        mModel.set(StatusProperties.STATUS_VIEW_TOOLTIP_TEXT, tooltipTextResId);
    }

    void setHoverHighlight(@DrawableRes int hoverHighlightResId) {
        mModel.set(StatusProperties.STATUS_VIEW_HOVER_HIGHLIGHT, hoverHighlightResId);
    }

    public void onUrlChanged() {
        var currentTab = mLocationBarDataProvider.getTab();
        if (mProfileSupplier.hasValue() && currentTab != null) {
            WebContents webContents = currentTab.getWebContents();
            Profile profile = mProfileSupplier.get();

            if (webContents != null && profile != null) {
                BrowserContextHandle originalBrowserContext =
                        profile.isOffTheRecord() ? profile.getOriginalProfile() : null;
                if (mCookieControlsBridge == null) {
                    mCookieControlsBridge =
                            new CookieControlsBridge(this, webContents, originalBrowserContext);
                } else if (mLastTabId != currentTab.getId() || mCurrentTabCrashed) {
                    mCookieControlsBridge.updateWebContents(webContents, originalBrowserContext);
                    mCurrentTabCrashed = false;
                }
            }
            mLastTabId = currentTab.getId();
        }
    }

    public void onPageLoadStopped() {
        Profile profile = mProfileSupplier.get();
        if (profile == null) {
            return;
        }
        if (mPageSecurityLevel != ConnectionSecurityLevel.SECURE) {
            return;
        }
        if (mBlockingStatus3pcd != CookieBlocking3pcdStatus.NOT_IN3PCD) {
            if (!mCookieControlsVisible || !mThirdPartyCookiesBlocked) return;

            if (UserPrefs.get(profile).getInteger(Pref.TRACKING_PROTECTION_ONBOARDING_ACK_ACTION)
                    == 0) {
                return;
            }

            animateCookieControlsIcon(() -> {});
        }
    }

    public void onTabCrashed() {
        mCurrentTabCrashed = true;
    }

    void setShowStatusView(boolean show) {
        mModel.set(StatusProperties.SHOW_STATUS_VIEW, show);
    }
}
