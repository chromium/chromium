// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import android.animation.Animator;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelAnimatorFactory;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * A component for displaying a status icon (e.g. security icon or navigation icon) and optional
 * verbose status text.
 */
public class StatusCoordinator implements View.OnClickListener, LocationBarDataProvider.Observer {
    /** Interface for displaying page info popup on omnibox. */
    public interface PageInfoAction {
        /**
         * @param tab Tab containing the content to show page info for.
         * @param pageInfoHighlight Providing the highlight row info related to this dialog.
         */
        void show(Tab tab, ChromePageInfoHighlight pageInfoHighlight);
    }

    // TODO(crbug.com/40707964): Do not store the StatusView
    private final StatusView mStatusView;
    private final StatusMediator mMediator;
    private final PropertyModel mModel;
    private final boolean mIsTablet;
    private final PageInfoAction mPageInfoAction;
    private LocationBarDataProvider mLocationBarDataProvider;
    private boolean mUrlHasFocus;

    /**
     * Creates a new {@link StatusCoordinator}.
     *
     * @param isTablet Whether the UI is shown on a tablet.
     * @param statusView The status view, used to supply and manipulate child views.
     * @param urlBarEditingTextStateProvider The url coordinator.
     * @param templateUrlServiceSupplier A supplier for {@link TemplateUrlService} used to query the
     *     default search engine.
     * @param windowAndroid The {@link WindowAndroid} that is used by the owning {@link Activity}.
     * @param pageInfoAction Displays page info popup.
     * @param merchantTrustSignalsCoordinatorSupplier Supplier of {@link
     *     MerchantTrustSignalsCoordinator}. Can be null if a store icon shouldn't be shown, such as
     *     when called from a search activity.
     * @param browserControlsVisibilityDelegate Delegate interface allowing control of the
     *     visibility of the browser controls (i.e. toolbar).
     */
    public StatusCoordinator(
            boolean isTablet,
            StatusView statusView,
            UrlBarEditingTextStateProvider urlBarEditingTextStateProvider,
            LocationBarDataProvider locationBarDataProvider,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            Supplier<Profile> profileSupplier,
            WindowAndroid windowAndroid,
            PageInfoAction pageInfoAction,
            @Nullable
                    Supplier<MerchantTrustSignalsCoordinator>
                            merchantTrustSignalsCoordinatorSupplier,
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate) {
        mIsTablet = isTablet;
        mStatusView = statusView;
        mLocationBarDataProvider = locationBarDataProvider;
        mPageInfoAction = pageInfoAction;

        mModel = new PropertyModel(StatusProperties.ALL_KEYS);

        PropertyModelChangeProcessor.create(mModel, mStatusView, new StatusViewBinder());

        PageInfoIPHController pageInfoIPHController =
                new PageInfoIPHController(
                        new UserEducationHelper(
                                ContextUtils.activityFromContext(mStatusView.getContext()),
                                profileSupplier,
                                new Handler(Looper.getMainLooper())),
                        getSecurityIconView());

        mMediator =
                new StatusMediator(
                        mModel,
                        mStatusView.getContext(),
                        urlBarEditingTextStateProvider,
                        isTablet,
                        locationBarDataProvider,
                        PermissionDialogController.getInstance(),
                        templateUrlServiceSupplier,
                        profileSupplier,
                        pageInfoIPHController,
                        windowAndroid,
                        merchantTrustSignalsCoordinatorSupplier);

        Resources res = mStatusView.getResources();
        mMediator.setUrlMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_min_url_width)
                        + res.getDimensionPixelSize(R.dimen.location_bar_status_icon_bg_size)
                        + res.getDimensionPixelSize(R.dimen.location_bar_start_padding)
                        + res.getDimensionPixelSize(R.dimen.location_bar_end_padding));

        mMediator.setSeparatorFieldMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_status_separator_width)
                        + res.getDimensionPixelSize(R.dimen.location_bar_status_separator_spacer));

        mMediator.setVerboseStatusTextMinWidth(
                res.getDimensionPixelSize(R.dimen.location_bar_min_verbose_status_text_width));

        // Update status immediately after receiving the data provider to avoid initial presence
        // glitch on tablet devices. This glitch would be typically seen upon launch of app, right
        // before the landing page is presented to the user.
        updateSecurityIcon();
        updateVerboseStatusVisibility();
        mLocationBarDataProvider.addObserver(this);
        mStatusView.setBrowserControlsVisibilityDelegate(browserControlsVisibilityDelegate);

        if (isSearchEngineStatusIconVisible()) {
            setTooltipText(R.string.accessibility_menu_info);
            setHoverHighlight(R.drawable.status_view_ripple);
        } else {
            setTooltipText(Resources.ID_NULL);
            setHoverHighlight(Resources.ID_NULL);
        }
    }

    /** Signals that native initialization has completed. */
    public void onNativeInitialized() {
        mMediator.updateLocationBarIcon(StatusView.IconTransitionType.CROSSFADE);
        mMediator.setStatusClickListener(this);
        mMediator.updateStatusVisibility();
        mMediator.setStoreIconController();
    }

    /**
     * @param urlHasFocus Whether the url currently has focus.
     */
    public void onUrlFocusChange(boolean urlHasFocus) {
        mMediator.setUrlHasFocus(urlHasFocus);
        mUrlHasFocus = urlHasFocus;
        updateVerboseStatusVisibility();
    }

    /**
     * @param show Whether the status icon should be VISIBLE, otherwise GONE.
     */
    public void setStatusIconShown(boolean show) {
        mMediator.setStatusIconShown(show);
    }

    /**
     * @param show Whether the status icon background should be VISIBLE, otherwise INVISIBLE.
     */
    public void setStatusIconBackgroundVisibility(boolean show) {
        mMediator.setStatusIconBackgroundVisibility(show);
    }

    /**
     * Set the url focus change percent.
     *
     * @param percent The current focus percent.
     */
    public void setUrlFocusChangePercent(float percent) {
        mMediator.setUrlFocusChangePercent(percent);
    }

    /** Set the x translation of the status view. */
    public void setTranslationX(float translationX) {
        mMediator.setTranslationX(translationX);
    }

    /** Set the tooltip text of the status view. */
    public void setTooltipText(@StringRes int tooltipTextResId) {
        mMediator.setTooltipText(tooltipTextResId);
    }

    /** Set the hover highlight of the status view. */
    public void setHoverHighlight(@DrawableRes int hoverHighlightResId) {
        mMediator.setHoverHighlight(hoverHighlightResId);
    }

    /**
     * @param brandedColorScheme The {@link BrandedColorScheme} to use for the status icon and text.
     */
    public void setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
        mMediator.setBrandedColorScheme(brandedColorScheme);

        // TODO(ender): remove this once icon selection has complete set of
        // corresponding properties (for tinting etc).
        updateSecurityIcon();
    }

    // LocationBarData.Observer implementation.
    // Using the default empty onPrimaryColorChanged.
    // Using the default empty onTitleChanged.
    // Using the default empty onUrlChanged.

    @Override
    public void onNtpStartedLoading() {
        mMediator.updateStatusVisibility();
    }

    @Override
    public void onIncognitoStateChanged() {
        mMediator.onIncognitoStateChanged();
    }

    @Override
    public void onSecurityStateChanged() {
        updateSecurityIcon();
        updateVerboseStatusVisibility();
    }

    @Override
    public void onUrlChanged() {
        mMediator.onUrlChanged();
    }

    @Override
    public void onPageLoadStopped() {
        mMediator.onPageLoadStopped();
    }

    @Override
    public void onTabCrashed() {
        mMediator.onTabCrashed();
    }

    /** Returns the resource identifier of the current security icon drawable. */
    public @DrawableRes int getSecurityIconResource() {
        return mMediator.getSecurityIconResource();
    }

    /** Updates the security icon displayed in the LocationBar. */
    private void updateSecurityIcon() {
        mMediator.updateSecurityIcon(
                mLocationBarDataProvider.getSecurityIconResource(mIsTablet),
                mLocationBarDataProvider.getSecurityIconColorStateList(),
                mLocationBarDataProvider.getSecurityIconContentDescriptionResourceId());
    }

    /** Returns the view displaying the security icon. */
    public View getSecurityIconView() {
        return mStatusView.getSecurityView();
    }

    /** Returns {@code true} if the security button is currently being displayed. */
    @VisibleForTesting
    public boolean isSecurityViewShown() {
        return mMediator.isSecurityViewShown();
    }

    /** Returns {@code true} if the search engine status is currently being displayed. */
    public boolean isSearchEngineStatusIconVisible() {
        // TODO(crbug.com/40707964): try to hide this method
        return mStatusView.isSearchEngineStatusIconVisible();
    }

    /** Returns {@code true} if the search engine icon is currently being displayed. */
    public boolean shouldDisplaySearchEngineIcon() {
        return mMediator.shouldDisplaySearchEngineIcon();
    }

    /** Returns the ID of the drawable currently shown in the security icon. */
    public @DrawableRes int getSecurityIconResourceIdForTesting() {
        return mModel.get(StatusProperties.STATUS_ICON_RESOURCE) == null
                ? 0
                : mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting();
    }

    /** Returns the icon identifier used for custom resources. */
    public String getSecurityIconIdentifierForTesting() {
        return mModel.get(StatusProperties.STATUS_ICON_RESOURCE) == null
                ? null
                : mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconIdentifierForTesting();
    }

    /**
     * Update visibility of the verbose status based on the button type and focus state of the
     * omnibox.
     */
    private void updateVerboseStatusVisibility() {
        mMediator.updateVerboseStatus(
                mLocationBarDataProvider.getSecurityLevel(),
                mLocationBarDataProvider.isOfflinePage(),
                mLocationBarDataProvider.isPaintPreview());
    }

    @Override
    public void onClick(View view) {
        if (mUrlHasFocus) return;

        if (!mLocationBarDataProvider.hasTab()
                || mLocationBarDataProvider.getTab().getWebContents() == null) {
            return;
        }

        mPageInfoAction.show(mLocationBarDataProvider.getTab(), mMediator.getPageInfoHighlight());
        mMediator.onPageInfoOpened();
    }

    /**
     * Called to set the width of the location bar when the url bar is not focused. This value is
     * used to determine whether the verbose status text should be visible.
     *
     * @param width The unfocused location bar width.
     */
    public void setUnfocusedLocationBarWidth(int width) {
        mMediator.setUnfocusedLocationBarWidth(width);
    }

    /** Toggle animation of icon changes. */
    public void setShouldAnimateIconChanges(boolean shouldAnimate) {
        mMediator.setAnimationsEnabled(shouldAnimate);
    }

    /** Returns width of the status icon including start/end margins. */
    public int getStatusIconWidth() {
        // TODO(crbug.com/40707964): try to hide this method
        return mStatusView.getStatusIconWidth();
    }

    /**
     * @see View#getMeasuredWidth()
     */
    public int getMeasuredWidth() {
        // TODO(crbug.com/40707964): try to hide this method
        return mStatusView.getMeasuredWidth();
    }

    /**
     * Notifies StatusCoordinator that the default match for the currently entered autocomplete text
     * has been classified, indicating whether the default match is a search.
     *
     * @param defaultMatchIsSearch Whether the default match is a search.
     */
    public void onDefaultMatchClassified(boolean defaultMatchIsSearch) {
        mMediator.updateLocationBarIconForDefaultMatchCategory(defaultMatchIsSearch);
    }

    public void destroy() {
        mMediator.destroy();
        mLocationBarDataProvider.removeObserver(this);
        mLocationBarDataProvider = null;
    }

    /** Returns whether the status view is currently in the process of animating a change. */
    public boolean isStatusIconAnimating() {
        return mStatusView.isStatusIconAnimating();
    }

    /**
     * Populates an animation that fades =/unfades the entire StatusView container with the given
     * start delay and duration, adding it to the given list of animators.
     */
    public void populateFadeAnimation(
            List<Animator> animators, long startDelayMs, long durationMs, float targetAlpha) {
        if (mLocationBarDataProvider.isIncognitoBranded()) {
            Animator animator =
                    PropertyModelAnimatorFactory.ofFloat(
                                    mModel, StatusProperties.ALPHA, targetAlpha)
                            .setDuration(durationMs);
            animator.setStartDelay(startDelayMs);
            animators.add(animator);
        }
    }

    /**
     * Set whether the status view should be shown. If the view is not shown, the status view will
     * be permanently gone until it is updated through this method during the current lifecycle.
     */
    public void setShowStatusView(boolean show) {
        mMediator.setShowStatusView(show);
    }
}
