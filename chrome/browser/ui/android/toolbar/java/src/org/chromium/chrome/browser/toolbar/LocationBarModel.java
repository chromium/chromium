// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.util.LruCache;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.paint_preview.TabbedPaintPreview;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfPageType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.omnibox.SecurityStatusIcon;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.Objects;

/** Provides a way of accessing toolbar data and state. */
public class LocationBarModel implements ToolbarDataProvider, LocationBarDataProvider {
    private static final int LRU_CACHE_SIZE = 10;

    static class SpannableDisplayTextCacheKey {
        @NonNull private final String mUrl;
        @NonNull private final String mDisplayText;
        private final int mSecurityLevel;
        private final int mNonEmphasizedColor;
        private final int mEmphasizedColor;
        private final int mDangerColor;
        private final int mSecureColor;

        private SpannableDisplayTextCacheKey(
                @NonNull String url,
                @NonNull String displayText,
                int securityLevel,
                int nonEmphasizedColor,
                int emphasizedColor,
                int dangerColor,
                int secureColor) {
            mUrl = url;
            mDisplayText = displayText;
            mSecurityLevel = securityLevel;
            mNonEmphasizedColor = nonEmphasizedColor;
            mEmphasizedColor = emphasizedColor;
            mDangerColor = dangerColor;
            mSecureColor = secureColor;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            if (o == null || getClass() != o.getClass()) {
                return false;
            }
            SpannableDisplayTextCacheKey that = (SpannableDisplayTextCacheKey) o;
            return mSecurityLevel == that.mSecurityLevel
                    && mNonEmphasizedColor == that.mNonEmphasizedColor
                    && mEmphasizedColor == that.mEmphasizedColor
                    && mDangerColor == that.mDangerColor
                    && mSecureColor == that.mSecureColor
                    && mUrl.equals(that.mUrl)
                    && mDisplayText.equals(that.mDisplayText);
        }

        @Override
        public int hashCode() {
            return Objects.hash(
                    mUrl,
                    mDisplayText,
                    mSecurityLevel,
                    mNonEmphasizedColor,
                    mEmphasizedColor,
                    mDangerColor,
                    mSecureColor);
        }
    }

    /** Formats the given URL to the original one of a distillation. */
    @FunctionalInterface
    public interface UrlFormatter {
        String format(GURL url);
    }

    /** Offline-related status of a given content. */
    public interface OfflineStatus {
        /** Returns whether the WebContents is showing trusted offline page. */
        default boolean isShowingTrustedOfflinePage(Tab tab) {
            return false;
        }

        /** Checks if an offline page is shown for the tab. */
        default boolean isOfflinePage(Tab tab) {
            return false;
        }
    }

    private final Context mContext;
    private final NewTabPageDelegate mNtpDelegate;
    private final @NonNull UrlFormatter mUrlFormatter;
    private final @NonNull OfflineStatus mOfflineStatus;
    // Always null if optimizations are disabled. Otherwise, non-null and unchanging following
    // native init. Always tied to the original profile which is safe because no underlying
    // services have an incognito-specific instance.
    @Nullable private AutocompleteSchemeClassifier mChromeAutocompleteSchemeClassifier;
    @Nullable private Profile mProfile;
    private boolean mInitializedProfileDependentFeatures;

    @Nullable
    private LruCache<SpannableDisplayTextCacheKey, SpannableStringBuilder>
            mSpannableDisplayTextCache;

    private Tab mTab;
    private int mPrimaryColor;

    private boolean mIsIncognitoBranded;
    private boolean mIsOffTheRecord;
    private boolean mIsUsingBrandColor;

    private long mNativeLocationBarModelAndroid;
    private ObserverList<LocationBarDataProvider.Observer> mLocationBarDataObservers =
            new ObserverList<>();
    private ObserverList<ToolbarDataProvider.Observer> mToolbarDataObservers = new ObserverList<>();
    protected GURL mVisibleGurl = GURL.emptyGURL();
    protected String mFormattedFullUrl;
    protected String mUrlForDisplay;
    private boolean mOmniboxUpdatedConnectionSecurityIndicatorsEnabled;

    // notifyUrlChanged and notifySecurityStateChanged are usually called 3 times across a same
    // document navigation. The first call is usually necessary, which updates the UrlBar to reflect
    // the new url. All subsequent calls are spurious and can be avoided. This experiment involves
    // using the flags below to short circuit all calls after the UrlBar has already been updated.
    private boolean mIsInSameDocNav;
    private boolean mAlreadyUpdatedUrlBarForSameDocNav;
    private boolean mAlreadyChangedSecurityStateForSameDocNav;

    /**
     * Default constructor for this class.
     *
     * @param context The Context used for styling the toolbar visuals.
     * @param newTabPageDelegate Delegate used to access NTP.
     * @param urlFormatter Formatter returning the formatted version of the original version of URL
     *     of a distillation.
     * @param offlineStatus Offline-related status provider.
     * @param searchEngineUtils Utils to query the state of the search engine logos feature.
     */
    public LocationBarModel(
            Context context,
            NewTabPageDelegate newTabPageDelegate,
            @NonNull UrlFormatter urlFormatter,
            @NonNull OfflineStatus offlineStatus) {
        mContext = context;
        mNtpDelegate = newTabPageDelegate;
        mUrlFormatter = urlFormatter;
        mOfflineStatus = offlineStatus;
        mPrimaryColor = ChromeColors.getDefaultThemeColor(context, false);
        mUrlForDisplay = "";
        mFormattedFullUrl = "";
    }

    /** Handle any initialization that must occur after native has been initialized. */
    public void initializeWithNative() {
        mOmniboxUpdatedConnectionSecurityIndicatorsEnabled =
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS);
        mNativeLocationBarModelAndroid = LocationBarModelJni.get().init(LocationBarModel.this);
        mSpannableDisplayTextCache = new LruCache<>(LRU_CACHE_SIZE);
    }

    private void performProfileDependentInitializationIfRequired() {
        if (mInitializedProfileDependentFeatures) return;
        assert mProfile != null;
        mInitializedProfileDependentFeatures = true;

        mChromeAutocompleteSchemeClassifier =
                new ChromeAutocompleteSchemeClassifier(getProfile().getOriginalProfile());
        recalculateFormattedUrls();
    }

    /** Destroys the native LocationBarModel. */
    public void destroy() {
        if (mChromeAutocompleteSchemeClassifier != null) {
            mChromeAutocompleteSchemeClassifier.destroy();
            mChromeAutocompleteSchemeClassifier = null;
        }
        if (mNativeLocationBarModelAndroid == 0) return;
        LocationBarModelJni.get().destroy(mNativeLocationBarModelAndroid, LocationBarModel.this);
        mNativeLocationBarModelAndroid = 0;
    }

    /**
     * @return The currently active WebContents being used by the Toolbar.
     */
    @CalledByNative
    private WebContents getActiveWebContents() {
        if (!hasTab()) return null;
        return mTab.getWebContents();
    }

    /**
     * Sets the tab that contains the information to be displayed in the toolbar.
     *
     * @param tab The tab associated currently with the toolbar.
     * @param profile The profile associated with the currently selected model, which must match the
     *     passed in tab if non-null.
     */
    public void setTab(@Nullable Tab tab, @NonNull Profile profile) {
        assert tab == null || tab.getProfile() == profile;
        assert profile != null;

        mTab = tab;
        mProfile = profile;
        performProfileDependentInitializationIfRequired();

        boolean isOffTheRecord = profile.isOffTheRecord();
        boolean isIncognitoBranded = profile.isIncognitoBranded();

        if (mIsOffTheRecord != isOffTheRecord || mIsIncognitoBranded != isIncognitoBranded) {
            mIsOffTheRecord = isOffTheRecord;
            mIsIncognitoBranded = isIncognitoBranded;
            notifyIncognitoStateChanged();
        }

        updateUsingBrandColor();
        notifyTitleChanged();
        notifyUrlChanged();
        notifyPrimaryColorChanged();
        notifySecurityStateChanged();
    }

    @Override
    public Tab getTab() {
        return hasTab() ? mTab : null;
    }

    @Override
    public boolean hasTab() {
        // TODO(crbug.com/40730536): Remove the isInitialized() and isDestroyed checks when
        // we no longer wait for TAB_CLOSED events to remove this tab.  Otherwise there is a chance
        // we use this tab after {@link Tab#destroy()} is called.
        return mTab != null && mTab.isInitialized() && !mTab.isDestroyed();
    }

    @Override
    public void addObserver(LocationBarDataProvider.Observer observer) {
        mLocationBarDataObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(LocationBarDataProvider.Observer observer) {
        mLocationBarDataObservers.removeObserver(observer);
    }

    @Override
    public void addToolbarDataProviderObserver(ToolbarDataProvider.Observer observer) {
        mToolbarDataObservers.addObserver(observer);
    }

    @Override
    public void removeToolbarDataProviderObserver(ToolbarDataProvider.Observer observer) {
        mToolbarDataObservers.removeObserver(observer);
    }

    @Override
    // TODO(crbug.com/40218072): migrate to GURL.
    @Deprecated
    public String getCurrentUrl() {
        return getCurrentGurl().getSpec().trim();
    }

    @Override
    public GURL getCurrentGurl() {
        return mVisibleGurl;
    }

    /**
     * Reterived updated cached values for the current URL.
     *
     * @return whether the URL value has changed.
     */
    @VisibleForTesting
    boolean updateVisibleGurl() {
        try (TraceEvent te = TraceEvent.scoped("LocationBarModel.updateVisibleGurl")) {
            GURL gurl = getUrlOfVisibleNavigationEntry();
            if (!gurl.equals(mVisibleGurl)) {
                mVisibleGurl = gurl;
                recalculateFormattedUrls();
                return true;
            }
        }
        return false;
    }

    public void notifyUrlChanged() {
        if ((mIsInSameDocNav && mAlreadyUpdatedUrlBarForSameDocNav) || !updateVisibleGurl()) {
            return;
        }

        // Url has changed, propagate it.
        for (LocationBarDataProvider.Observer observer : mLocationBarDataObservers) {
            observer.onUrlChanged();
        }

        mAlreadyUpdatedUrlBarForSameDocNav = mIsInSameDocNav;
    }

    public void notifyZeroSuggestRefresh() {
        for (LocationBarDataProvider.Observer observer : mLocationBarDataObservers) {
            observer.hintZeroSuggestRefresh();
        }
    }

    @Override
    public NewTabPageDelegate getNewTabPageDelegate() {
        return mNtpDelegate;
    }

    void notifyNtpStartedLoading() {
        for (LocationBarDataProvider.Observer observer : mLocationBarDataObservers) {
            observer.onNtpStartedLoading();
        }
    }

    @Override
    public UrlBarData getUrlBarData() {
        // Part of scroll jank investigation http://crbug.com/905461. Will remove TraceEvent after
        // the investigation is complete.
        try (TraceEvent te = TraceEvent.scoped("LocationBarModel.getUrlBarData")) {
            if (!hasTab()) {
                return UrlBarData.EMPTY;
            }

            GURL gurl = getCurrentGurl();
            if (!UrlBarData.shouldShowUrl(gurl, isOffTheRecord())) {
                return UrlBarData.EMPTY;
            }

            String url = gurl.getSpec().trim();
            boolean isOfflinePage = isOfflinePage();
            String formattedUrl = getFormattedFullUrl();
            if (mTab.isFrozen()) return buildUrlBarData(gurl, isOfflinePage, formattedUrl);

            if (DomDistillerUrlUtils.isDistilledPage(url)) {
                GURL originalUrl =
                        DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(new GURL(url));
                return buildUrlBarData(originalUrl, isOfflinePage);
            }

            if (isOfflinePage) {
                GURL originalUrl = mTab.getOriginalUrl();
                formattedUrl = UrlUtilities.stripScheme(mUrlFormatter.format(originalUrl));

                // Clear the editing text for untrusted offline pages.
                if (!mOfflineStatus.isShowingTrustedOfflinePage(mTab)) {
                    return buildUrlBarData(gurl, true, formattedUrl, "");
                }

                return buildUrlBarData(gurl, true, formattedUrl);
            }

            String urlForDisplay = getUrlForDisplay();
            if (!urlForDisplay.equals(formattedUrl)) {
                return buildUrlBarData(gurl, false, urlForDisplay, formattedUrl);
            }

            return buildUrlBarData(gurl, false, formattedUrl);
        }
    }

    private UrlBarData buildUrlBarData(GURL url, boolean isOfflinePage) {
        return buildUrlBarData(url, isOfflinePage, url.getSpec());
    }

    private UrlBarData buildUrlBarData(GURL url, boolean isOfflinePage, String displayText) {
        return buildUrlBarData(url, isOfflinePage, displayText, displayText);
    }

    private UrlBarData buildUrlBarData(
            GURL url, boolean isOfflinePage, String displayText, String editingText) {
        SpannableStringBuilder spannableDisplayText = null;
        if (mNativeLocationBarModelAndroid != 0
                && displayText != null
                && displayText.length() > 0
                && shouldEmphasizeUrl()) {
            final @BrandedColorScheme int brandedColorScheme =
                    OmniboxResourceProvider.getBrandedColorScheme(
                            mContext, isIncognitoBranded(), getPrimaryColor());
            final @ColorInt int nonEmphasizedColor =
                    OmniboxResourceProvider.getUrlBarSecondaryTextColor(
                            mContext, brandedColorScheme);
            final @ColorInt int emphasizedColor =
                    OmniboxResourceProvider.getUrlBarPrimaryTextColor(mContext, brandedColorScheme);
            final @ColorInt int dangerColor =
                    OmniboxResourceProvider.getUrlBarDangerColor(mContext, brandedColorScheme);
            final @ColorInt int secureColor =
                    OmniboxResourceProvider.getUrlBarSecureColor(mContext, brandedColorScheme);

            AutocompleteSchemeClassifier autocompleteSchemeClassifier;
            int securityLevel = getSecurityLevel(getTab(), isOfflinePage);
            SpannableDisplayTextCacheKey cacheKey =
                    new SpannableDisplayTextCacheKey(
                            url.getSpec(),
                            displayText,
                            securityLevel,
                            nonEmphasizedColor,
                            emphasizedColor,
                            dangerColor,
                            secureColor);
            SpannableStringBuilder cachedSpannableDisplayText =
                    mSpannableDisplayTextCache.get(cacheKey);
            autocompleteSchemeClassifier = mChromeAutocompleteSchemeClassifier;

            if (cachedSpannableDisplayText != null) {
                return UrlBarData.forUrlAndText(url, cachedSpannableDisplayText, editingText);
            } else {
                spannableDisplayText = new SpannableStringBuilder(displayText);
                OmniboxUrlEmphasizer.emphasizeUrl(
                        spannableDisplayText,
                        autocompleteSchemeClassifier,
                        getSecurityLevel(),
                        shouldEmphasizeHttpsScheme(),
                        nonEmphasizedColor,
                        emphasizedColor,
                        dangerColor,
                        secureColor);
                mSpannableDisplayTextCache.put(cacheKey, spannableDisplayText);
            }
        }
        return UrlBarData.forUrlAndText(url, spannableDisplayText, editingText);
    }

    /**
     * @return True if the displayed URL should be emphasized, false if the displayed text
     *         already has formatting for emphasis applied.
     */
    private boolean shouldEmphasizeUrl() {
        // If the toolbar shows the publisher URL, it applies its own formatting for emphasis.
        if (mTab == null) return true;

        return TrustedCdn.getPublisherUrl(mTab) == null;
    }

    LruCache<SpannableDisplayTextCacheKey, SpannableStringBuilder> getCacheForTesting() {
        return mSpannableDisplayTextCache;
    }

    /**
     * @return Whether the light security theme should be used.
     */
    @VisibleForTesting
    public boolean shouldEmphasizeHttpsScheme() {
        return !isUsingBrandColor() && !isIncognitoBranded();
    }

    @Override
    public String getTitle() {
        if (!hasTab()) return "";

        String title = getTab().getTitle();
        return TextUtils.isEmpty(title) ? title : title.trim();
    }

    public void notifyTitleChanged() {
        for (LocationBarDataProvider.Observer observer : mLocationBarDataObservers) {
            observer.onTitleChanged();
        }
    }

    @Override
    public boolean isIncognito() {
        return mIsOffTheRecord;
    }

    @Override
    public boolean isIncognitoBranded() {
        return mIsIncognitoBranded;
    }

    @Override
    public boolean isOffTheRecord() {
        return mIsOffTheRecord;
    }

    private void notifyIncognitoStateChanged() {
        for (LocationBarDataProvider.Observer observer : mLocationBarDataObservers) {
            observer.onIncognitoStateChanged();
        }

        for (ToolbarDataProvider.Observer observer : mToolbarDataObservers) {
            observer.onIncognitoStateChanged();
        }
    }

    @Override
    public Profile getProfile() {
        return mProfile;
    }

    /**
     * Sets the primary color and changes the state for isUsingBrandColor.
     *
     * @param color The primary color for the current tab.
     */
    public void setPrimaryColor(int color) {
        mPrimaryColor = color;
        updateUsingBrandColor();
        notifyPrimaryColorChanged();
    }

    private void updateUsingBrandColor() {
        mIsUsingBrandColor =
                !isIncognitoBranded()
                        && mPrimaryColor
                                != ChromeColors.getDefaultThemeColor(mContext, isIncognitoBranded())
                        && hasTab()
                        && !mTab.isNativePage();
    }

    @Override
    public int getPrimaryColor() {
        return mPrimaryColor;
    }

    @Override
    public boolean isUsingBrandColor() {
        return mIsUsingBrandColor;
    }

    public void notifyPrimaryColorChanged() {
        for (LocationBarDataProvider.Observer observer : mLocationBarDataObservers) {
            observer.onPrimaryColorChanged();
        }
    }

    @Override
    public boolean isOfflinePage() {
        return hasTab() && mOfflineStatus.isOfflinePage(mTab);
    }

    @Override
    public boolean isPaintPreview() {
        return hasTab() && TabbedPaintPreview.get(mTab).isShowing();
    }

    private int getPdfPageType() {
        if (!hasTab()) {
            return 0;
        }
        return PdfUtils.getPdfPageType(mTab.getNativePage());
    }

    @Override
    public int getSecurityLevel() {
        return getSecurityLevel(getTab(), isOfflinePage());
    }

    @Override
    public int getPageClassification(boolean isPrefetch) {
        if (mNativeLocationBarModelAndroid == 0) return PageClassification.INVALID_SPEC_VALUE;

        return LocationBarModelJni.get()
                .getPageClassification(mNativeLocationBarModelAndroid, isPrefetch);
    }

    @Override
    public @DrawableRes int getSecurityIconResource(boolean isTablet) {
        boolean isOfflinePage = isOfflinePage();
        return getSecurityIconResource(
                getSecurityLevel(getTab(), isOfflinePage),
                !isTablet,
                isOfflinePage,
                isPaintPreview(),
                getPdfPageType());
    }

    @Override
    public @StringRes int getSecurityIconContentDescriptionResourceId() {
        return SecurityStatusIcon.getSecurityIconContentDescriptionResourceId(getSecurityLevel());
    }

    @VisibleForTesting
    @ConnectionSecurityLevel
    int getSecurityLevel(Tab tab, boolean isOfflinePage) {
        if (tab == null || isOfflinePage) {
            return ConnectionSecurityLevel.NONE;
        }

        @Nullable GURL publisherUrl = TrustedCdn.getPublisherUrl(tab);

        if (publisherUrl != null) {
            assert getSecurityLevelFromStateModel(tab.getWebContents())
                    != ConnectionSecurityLevel.DANGEROUS;
            return publisherUrl.getScheme().equals(UrlConstants.HTTPS_SCHEME)
                    ? ConnectionSecurityLevel.SECURE
                    : ConnectionSecurityLevel.WARNING;
        }
        return getSecurityLevelFromStateModel(tab.getWebContents());
    }

    @VisibleForTesting
    @ConnectionSecurityLevel
    int getSecurityLevelFromStateModel(WebContents webContents) {
        int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(webContents);
        return securityLevel;
    }

    @VisibleForTesting
    @DrawableRes
    int getSecurityIconResource(
            int securityLevel,
            boolean isSmallDevice,
            boolean isOfflinePage,
            boolean isPaintPreview,
            int pdfPageType) {
        // Paint Preview appears on top of WebContents and shows a visual representation of the page
        // that has been previously stored locally.
        if (isPaintPreview) return R.drawable.omnibox_info;

        // Checking for a preview first because one possible preview type is showing an offline page
        // on a slow connection. In this case, the previews UI takes precedence.
        if (isOfflinePage) {
            return R.drawable.ic_offline_pin_24dp;
        }

        // Pdf page is a native page used to render downloaded pdf files.
        // Show warning icon for pdf from insecure source (e.g. mixed content download).
        if (pdfPageType == PdfPageType.TRANSIENT_INSECURE) {
            return R.drawable.omnibox_not_secure_warning;
        }
        // Show info icon for other pdf pages.
        if (pdfPageType == PdfPageType.TRANSIENT_SECURE || pdfPageType == PdfPageType.LOCAL) {
            return R.drawable.omnibox_info;
        }

        // Return early if native initialization hasn't been done yet.
        if ((securityLevel == ConnectionSecurityLevel.NONE
                        || securityLevel == ConnectionSecurityLevel.WARNING)
                && mNativeLocationBarModelAndroid == 0) {
            return R.drawable.omnibox_info;
        }

        boolean skipIconForNeutralState =
                (mProfile != null
                                && !SearchEngineUtils.getForProfile(mProfile)
                                        .shouldShowSearchEngineLogo())
                        || mNtpDelegate.isCurrentlyVisible();

        return SecurityStatusIcon.getSecurityIconResource(
                securityLevel,
                isSmallDevice,
                skipIconForNeutralState,
                mOmniboxUpdatedConnectionSecurityIndicatorsEnabled);
    }

    @Override
    public @ColorRes int getSecurityIconColorStateList() {
        final @ColorInt int color = getPrimaryColor();
        final @BrandedColorScheme int brandedColorScheme =
                OmniboxResourceProvider.getBrandedColorScheme(
                        mContext, isIncognitoBranded(), color);

        // Assign red color to security icon if the page shows security warning.
        return getSecurityIconColorWithSecurityLevel(
                getSecurityLevel(), brandedColorScheme, isIncognitoBranded());
    }

    /**
     * Get the color for the security icon for different security levels. If we are using dark
     * background (dark mode or incognito mode), we should return light red. If we are using light
     * background (light mode, but not LIGHT_BRANDED_THEME), we should return dark red. The default
     * brand color will be returned if no change is needed.
     *
     * @param connectionSecurityLevel The connection security level for the current website.
     * @param brandedColorScheme The branded color scheme for the omnibox.
     * @param isIncognito Whether the tab is in Incognito mode.
     * @return The color resource for the security icon, returns -1 if doe snot need to change
     *     color.
     */
    @VisibleForTesting
    protected @ColorRes int getSecurityIconColorWithSecurityLevel(
            @ConnectionSecurityLevel int connectionSecurityLevel,
            @BrandedColorScheme int brandedColorScheme,
            boolean isIncognito) {
        // Return regular color scheme if the website does not show warning.
        if (connectionSecurityLevel == ConnectionSecurityLevel.DANGEROUS) {
            // Assign red color only on light or dark background including Incognito mode.
            // We will not change the security icon to red when BrandedColorScheme is
            // LIGHT_BRANDED_THEME for the purpose of improving contrast.
            if (isIncognito) {
                // Use light red for Incognito mode.
                return R.color.baseline_error_80;
            } else if (brandedColorScheme == BrandedColorScheme.APP_DEFAULT) {
                // Use adaptive red for light and dark background.
                return R.color.default_red;
            }
        }
        return ThemeUtils.getThemedToolbarIconTintRes(brandedColorScheme);
    }

    public void notifySecurityStateChanged() {
        if (mIsInSameDocNav && mAlreadyChangedSecurityStateForSameDocNav) {
            return;
        }

        @ConnectionSecurityLevel int securityLevel = getSecurityLevel();
        if (securityLevel == ConnectionSecurityLevel.DANGEROUS) {
            recalculateFormattedUrls();
        }

        for (LocationBarDataProvider.Observer observer : mLocationBarDataObservers) {
            observer.onSecurityStateChanged();
        }

        mAlreadyChangedSecurityStateForSameDocNav = mIsInSameDocNav;
    }

    private void recalculateFormattedUrls() {
        mFormattedFullUrl = calculateFormattedFullUrl();
        mUrlForDisplay = calculateUrlForDisplay();
    }

    private String getFormattedFullUrl() {
        return mFormattedFullUrl;
    }

    private String getUrlForDisplay() {
        return mUrlForDisplay;
    }

    /** @return The formatted URL suitable for editing. */
    protected String calculateFormattedFullUrl() {
        if (mNativeLocationBarModelAndroid == 0) return "";
        return LocationBarModelJni.get()
                .getFormattedFullURL(mNativeLocationBarModelAndroid, LocationBarModel.this);
    }

    /** @return The formatted URL suitable for display only. */
    protected String calculateUrlForDisplay() {
        if (mNativeLocationBarModelAndroid == 0) return "";
        return LocationBarModelJni.get()
                .getURLForDisplay(mNativeLocationBarModelAndroid, LocationBarModel.this);
    }

    protected GURL getUrlOfVisibleNavigationEntry() {
        if (mNativeLocationBarModelAndroid == 0) return GURL.emptyGURL();
        if (mNtpDelegate.isCurrentlyVisible()) {
            return getTab().getUrl();
        }

        return LocationBarModelJni.get()
                .getUrlOfVisibleNavigationEntry(
                        mNativeLocationBarModelAndroid, LocationBarModel.this);
    }

    /** Notify changes for non static layout. */
    public void updateForNonStaticLayout() {
        notifyTitleChanged();
        notifyUrlChanged();
        notifyPrimaryColorChanged();
        notifySecurityStateChanged();
    }

    public void notifyDidStartNavigation(boolean isSameDocument) {
        resetSameDocNavFlags();
        mIsInSameDocNav = isSameDocument;
    }

    public void notifyDidFinishNavigationEnd() {
        resetSameDocNavFlags();
    }

    public void notifyOnCrash() {
        resetSameDocNavFlags();
        for (LocationBarDataProvider.Observer observer : mLocationBarDataObservers) {
            observer.onTabCrashed();
        }
    }

    public void notifyContentChanged() {
        resetSameDocNavFlags();
    }

    public void notifyWebContentsSwapped() {
        resetSameDocNavFlags();
    }

    private void resetSameDocNavFlags() {
        mIsInSameDocNav = false;
        mAlreadyUpdatedUrlBarForSameDocNav = false;
        mAlreadyChangedSecurityStateForSameDocNav = false;
    }

    @NativeMethods
    interface Natives {
        long init(LocationBarModel caller);

        void destroy(long nativeLocationBarModelAndroid, LocationBarModel caller);

        String getFormattedFullURL(long nativeLocationBarModelAndroid, LocationBarModel caller);

        String getURLForDisplay(long nativeLocationBarModelAndroid, LocationBarModel caller);

        GURL getUrlOfVisibleNavigationEntry(
                long nativeLocationBarModelAndroid, LocationBarModel caller);

        int getPageClassification(long nativeLocationBarModelAndroid, boolean isPrefetch);
    }

    public void onPageLoadStopped() {
        for (LocationBarDataProvider.Observer observer : mLocationBarDataObservers) {
            observer.onPageLoadStopped();
        }
    }
}
