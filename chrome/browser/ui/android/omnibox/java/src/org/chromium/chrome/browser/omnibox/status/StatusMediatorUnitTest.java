// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.omnibox.status.StatusMediator.COOKIE_CONTROLS_ICON;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;

import androidx.test.filters.SmallTest;

import com.google.android.material.color.MaterialColors;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusView.IconTransitionType;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.content_settings.CookieControlsBreakageConfidenceLevel;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsBridgeJni;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

/**
 * Unit tests for {@link StatusMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class StatusMediatorUnitTest {
    private static final String TAG = "StatusMediatorUnitTest";
    private static final String TEST_SEARCH_URL = "https://www.test.com";

    public @Rule TestRule mProcessor = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule JniMocker mJniMocker = new JniMocker();

    private @Mock NewTabPageDelegate mNewTabPageDelegate;
    private @Mock LocationBarDataProvider mLocationBarDataProvider;
    private @Mock UrlBarEditingTextStateProvider mUrlBarEditingTextStateProvider;
    private @Mock SearchEngineLogoUtils mSearchEngineLogoUtils;
    private @Mock Profile mProfile;
    private @Mock TemplateUrlService mTemplateUrlService;
    private @Mock PermissionDialogController mPermissionDialogController;
    private @Mock PageInfoIPHController mPageInfoIPHController;
    private @Mock MerchantTrustSignalsCoordinator mMerchantTrustSignalsCoordinator;
    private @Mock Drawable mStoreIconDrawable;

    private @Mock CookieControlsBridge mCookieControlsBridge;

    private @Mock CookieControlsBridge.Natives mCookieControlsBridgeJniMock;

    private @Mock Tab mTab;

    private @Mock WebContents mWebContents;

    Context mContext;
    Resources mResources;

    PropertyModel mModel;
    StatusMediator mMediator;
    OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier;
    WindowAndroid mWindowAndroid;

    @Before
    public void setUp() {
        mContext = new ContextThemeWrapper(
                ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mResources = mContext.getResources();
        mWindowAndroid = new WindowAndroid(mContext);

        mModel = new PropertyModel(StatusProperties.ALL_KEYS);

        mJniMocker.mock(CookieControlsBridgeJni.TEST_HOOKS, mCookieControlsBridgeJniMock);

        // By default return google g, but this behavior is overridden in some tests.
        Promise<StatusIconResource> logoPromise =
                Promise.fulfilled(new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0));

        doReturn(false).when(mLocationBarDataProvider).isInOverviewAndShowingOmnibox();
        doReturn(false).when(mLocationBarDataProvider).isIncognito();
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(logoPromise)
                .when(mSearchEngineLogoUtils)
                .getSearchEngineLogo(
                        eq(mResources), eq(BrandedColorScheme.APP_DEFAULT), any(), any());

        setupStatusMediator(/* isTablet= */ false);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    private void setupStatusMediator(boolean isTablet) {
        mTemplateUrlServiceSupplier = new OneshotSupplierImpl<>();
        ObservableSupplierImpl<MerchantTrustSignalsCoordinator>
                merchantTrustSignalsCoordinatorObservableSupplier = new ObservableSupplierImpl<>();
        mMediator = new StatusMediator(mModel, mResources, mContext,
                mUrlBarEditingTextStateProvider, isTablet, mLocationBarDataProvider,
                mPermissionDialogController, mSearchEngineLogoUtils, mTemplateUrlServiceSupplier,
                ()
                        -> mProfile,
                mPageInfoIPHController, mWindowAndroid,
                merchantTrustSignalsCoordinatorObservableSupplier);
        mTemplateUrlServiceSupplier.set(mTemplateUrlService);
        merchantTrustSignalsCoordinatorObservableSupplier.set(mMerchantTrustSignalsCoordinator);
    }

    @Test
    @SmallTest
    public void searchEngineLogo_isGoogleLogo() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_isGoogleLogo_hideAfterUnfocusFinished() {
        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();

        mMediator.setUrlHasFocus(true);
        mMediator.setUrlHasFocus(false);
        Assert.assertFalse(mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @SmallTest
    public void searchEngineLogo_isGoogleLogo_noHideIconAfterUnfocusedWhenScrolled() {
        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setUrlFocusChangePercent(1f);
        mMediator.setUrlHasFocus(true);
        mMediator.setUrlHasFocus(false);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @SmallTest
    public void searchEngineLogo_isGoogleLogoOnNtp() {
        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
        Assert.assertFalse(mMediator.shouldDisplaySearchEngineIcon());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_isGoogleLogoOnNtpTablet() {
        setupStatusMediator(/* isTablet= */ true);
        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();

        mMediator.setUrlHasFocus(false);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
        Assert.assertFalse(mMediator.shouldDisplaySearchEngineIcon());
    }

    @Test
    @SmallTest
    public void searchEngineLogoTablet() {
        setupStatusMediator(/* isTablet= */ true);
        mMediator.setUrlHasFocus(true);

        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
        Assert.assertTrue(mMediator.shouldDisplaySearchEngineIcon());

        doReturn(true).when(mLocationBarDataProvider).isIncognito();
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
        Assert.assertFalse(mMediator.shouldDisplaySearchEngineIcon());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_isGoogleLogo_whenScrolled() {
        doReturn(false).when(mLocationBarDataProvider).isLoading();
        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setUrlFocusChangePercent(1f);
        Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenTextIsSite() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();

        mMediator.updateLocationBarIconForDefaultMatchCategory(false);
        Assert.assertEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenAutocompleteSiteContainsText() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();

        mMediator.updateLocationBarIconForDefaultMatchCategory(false);
        Assert.assertEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_onTextChanged_noGlobeReplacementWhenUrlBarTextDoesNotMatch() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();

        mMediator.updateLocationBarIconForDefaultMatchCategory(true);
        Assert.assertNotEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_onTextChanged_noGlobeReplacementWhenUrlBarTextIsEmpty() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        // Setup a valid url to prevent the default "" from matching the url.
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();

        mMediator.updateLocationBarIconForDefaultMatchCategory(false);
        mMediator.updateLocationBarIconForDefaultMatchCategory(true);
        Assert.assertNotEquals(R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_incognitoStateChanged() {
        mMediator.onIncognitoStateChanged();

        Assert.assertEquals(false, mModel.get(StatusProperties.SHOW_STATUS_ICON));
        Assert.assertEquals(1f, mModel.get(StatusProperties.STATUS_ICON_ALPHA), 0f);
    }

    @Test
    @SmallTest
    public void searchEngineLogo_incognitoNoIcon() {
        doReturn(true).when(mLocationBarDataProvider).isIncognito();

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSecurityIcon(0, 0, 0);

        Assert.assertEquals(null, mModel.get(StatusProperties.STATUS_ICON_RESOURCE));
    }

    @Test
    @SmallTest
    public void searchEngineLogo_maybeUpdateStatusIconForSearchEngineIconChanges() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.updateSecurityIcon(0, 0, 0);

        Assert.assertTrue(mMediator.maybeUpdateStatusIconForSearchEngineIcon());
        Assert.assertEquals(R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_maybeUpdateStatusIconForSearchEngineIconNoChanges() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(false);
        mMediator.updateSecurityIcon(0, 0, 0);

        Assert.assertFalse(mMediator.maybeUpdateStatusIconForSearchEngineIcon());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_intermediateUrlFocusPercent() {
        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();
        mMediator.setUrlFocusChangePercent(0f);

        Assert.assertEquals(false, mModel.get(StatusProperties.SHOW_STATUS_ICON));

        mMediator.setUrlFocusChangePercent(0.1f);
        Assert.assertEquals(true, mModel.get(StatusProperties.SHOW_STATUS_ICON));

        mMediator.setUrlFocusChangePercent(0.4f);
        Assert.assertEquals(true, mModel.get(StatusProperties.SHOW_STATUS_ICON));

        mMediator.setUrlFocusChangePercent(0.7f);
        Assert.assertEquals(true, mModel.get(StatusProperties.SHOW_STATUS_ICON));

        mMediator.setUrlFocusChangePercent(0.9f);
        Assert.assertEquals(true, mModel.get(StatusProperties.SHOW_STATUS_ICON));

        verify(mSearchEngineLogoUtils, times(1))
                .getSearchEngineLogo(
                        eq(mResources), eq(BrandedColorScheme.APP_DEFAULT), any(), any());

        mMediator.setUrlFocusChangePercent(0.0f);
        Assert.assertEquals(false, mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @SmallTest
    public void resolveUrlBarTextWithAutocomplete_urlBarTextEmpty() {
        Assert.assertEquals("Empty urlBarText should resolve to empty urlBarTextWithAutocomplete",
                "", mMediator.resolveUrlBarTextWithAutocomplete(""));
    }

    @Test
    @SmallTest
    public void resolveUrlBarTextWithAutocomplete_urlBarTextMismatchesAutocompleteText() {
        doReturn("https://foo.com").when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        String msg = "The urlBarText should only resolve to the autocomplete text if it's a "
                + "substring of the autocomplete text.";
        Assert.assertEquals(
                msg, "https://foo.com", mMediator.resolveUrlBarTextWithAutocomplete("foo.com"));
        Assert.assertEquals(msg, "bar.com", mMediator.resolveUrlBarTextWithAutocomplete("bar.com"));
    }

    @Test
    @SmallTest
    public void testIncognitoStateChange_goingToIncognito() {
        mMediator.setShowIconsWhenUrlFocused(true);

        doReturn(true).when(mLocationBarDataProvider).isIncognito();
        mMediator.onIncognitoStateChanged();
        Assert.assertEquals(null, mModel.get(StatusProperties.STATUS_ICON_RESOURCE));
        Assert.assertEquals(1f, mModel.get(StatusProperties.STATUS_ICON_ALPHA), 0f);
    }

    @Test
    @SmallTest
    public void testIncognitoStateChange_backFromIncognito() {
        mMediator.setShowIconsWhenUrlFocused(true);

        doReturn(true).when(mLocationBarDataProvider).isIncognito();
        mMediator.onIncognitoStateChanged();
        doReturn(false).when(mLocationBarDataProvider).isIncognito();
        mMediator.onIncognitoStateChanged();
        Assert.assertEquals(null, mModel.get(StatusProperties.STATUS_ICON_RESOURCE));
        Assert.assertEquals(1f, mModel.get(StatusProperties.STATUS_ICON_ALPHA), 0f);
    }

    @Test
    @SmallTest
    public void testStatusText() {
        mMediator.setUnfocusedLocationBarWidth(10);
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, true, true);
        // When both states, offline, and preview are enabled, paint preview has
        // the highest priority.
        Assert.assertEquals("Incorrect text for paint preview status text",
                R.string.location_bar_paint_preview_page_status,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        Assert.assertEquals("Incorrect color for paint preview status text",
                MaterialColors.getColor(mContext, R.attr.colorPrimary, TAG),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
        mMediator.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        Assert.assertEquals("Incorrect color for paint preview status text",
                mContext.getColor(R.color.locationbar_status_preview_color_dark),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));

        // When only offline is enabled, it should be shown.
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, true, false);
        mMediator.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        Assert.assertEquals("Incorrect text for offline page status text",
                R.string.location_bar_verbose_status_offline,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        Assert.assertEquals("Incorrect color for offline page status text",
                mContext.getColor(R.color.locationbar_status_offline_color_light),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
        mMediator.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        Assert.assertEquals("Incorrect color for offline page status text",
                mContext.getColor(R.color.locationbar_status_offline_color_dark),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
    }

    @Test
    @SmallTest
    public void testTemplateUrlServiceChanged() {
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setUrlHasFocus(true);

        mMediator.onTemplateURLServiceChanged();
        verify(mSearchEngineLogoUtils, times(2))
                .getSearchEngineLogo(
                        eq(mResources), eq(BrandedColorScheme.APP_DEFAULT), any(), any());
    }

    @Test
    @SmallTest
    public void testSetStoreIconController() {
        mMediator.setStoreIconController();
        verify(mMerchantTrustSignalsCoordinator, times(1)).setOmniboxIconController(eq(mMediator));
    }

    @Test
    @SmallTest
    public void testShowStoreIcon_DifferentUrl() {
        setupStoreIconForTesting(false);
        // Show the default icon first.
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertFalse(mMediator.isStoreIconShowing());

        // Try to show the store icon.
        mMediator.showStoreIcon(mWindowAndroid, "test2.com", mStoreIconDrawable, 0, true);
        Assert.assertFalse(mMediator.isStoreIconShowing());
    }

    @Test
    @SmallTest
    public void testShowStoreIcon_InIncognito() {
        setupStoreIconForTesting(true);
        // Show the default icon first.
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertFalse(mMediator.isStoreIconShowing());

        // Try to show the store icon.
        mMediator.showStoreIcon(
                mWindowAndroid, JUnitTestGURLs.BLUE_1.getSpec(), mStoreIconDrawable, 0, true);
        Assert.assertFalse(mMediator.isStoreIconShowing());
    }

    @Test
    @SmallTest
    public void testShowStoreIcon() {
        setupStoreIconForTesting(false);
        // Show the default icon first.
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertFalse(mMediator.isStoreIconShowing());

        // Try to show the store icon.
        mMediator.showStoreIcon(
                mWindowAndroid, JUnitTestGURLs.BLUE_1.getSpec(), mStoreIconDrawable, 0, true);
        Assert.assertTrue(mMediator.isStoreIconShowing());
        Assert.assertEquals(IconTransitionType.ROTATE,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getTransitionType());
        Assert.assertNotNull(
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback());
        mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback().run();
        verify(mPageInfoIPHController, times(1)).showStoreIconIPH(anyInt(), eq(0));

        // Simulate that the store icon is blown away by other customized icon.
        mMediator.resetCustomIconsStatus();
        Assert.assertFalse(mMediator.isStoreIconShowing());

        // Show store icon again.
        mMediator.showStoreIcon(
                mWindowAndroid, JUnitTestGURLs.BLUE_1.getSpec(), mStoreIconDrawable, 0, true);
        Assert.assertTrue(mMediator.isStoreIconShowing());

        // Simulate that we need to switch back to the default icon.
        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);
        Assert.assertFalse(mMediator.isStoreIconShowing());
    }

    @Test
    @SmallTest
    public void testShowStoreIcon_NotEligibleToShowIph() {
        setupStoreIconForTesting(false);
        // Show the default icon first.
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertFalse(mMediator.isStoreIconShowing());

        // Try to show the store icon.
        mMediator.showStoreIcon(
                mWindowAndroid, JUnitTestGURLs.BLUE_1.getSpec(), mStoreIconDrawable, 0, false);
        Assert.assertTrue(mMediator.isStoreIconShowing());
        Assert.assertEquals(IconTransitionType.ROTATE,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getTransitionType());
        Assert.assertNotNull(
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback());
        mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback().run();
        verify(mPageInfoIPHController, times(0)).showStoreIconIPH(anyInt(), eq(0));
    }

    @Test
    @SmallTest
    public void searchEngineLogo_startSurface() {
        doReturn(false).when(mNewTabPageDelegate).isCurrentlyVisible();
        doReturn(true).when(mLocationBarDataProvider).isInOverviewAndShowingOmnibox();

        mMediator.setUrlHasFocus(false);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_ICON));
        Assert.assertFalse(mMediator.shouldDisplaySearchEngineIcon());

        mMediator.setUrlFocusChangePercent(0.5f);
        Assert.assertTrue(mMediator.shouldDisplaySearchEngineIcon());
    }

    @Test
    @SmallTest
    public void testCookieControlsIcon_animateOnPageStoppedLoading() {
        mMediator.setUrlHasFocus(true);
        mMediator.setCookieControlsBridge(mCookieControlsBridge);

        Assert.assertNotEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());

        mMediator.onBreakageConfidenceLevelChanged(CookieControlsBreakageConfidenceLevel.HIGH);

        Assert.assertNotEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());

        mMediator.onPageLoadStopped();
        Assert.assertEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());

        mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback().run();
        verify(mPageInfoIPHController, times(1)).showCookieControlsIPH(anyInt(), anyInt());
        verify(mCookieControlsBridge, times(1)).onEntryPointAnimated();

        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);

        // CookieControlsIcon should not be set when no HIGH BreakageConfidenceLevel were
        // explicitly reported.
        mMediator.onPageLoadStopped();
        Assert.assertNotEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());
    }

    @Test
    @SmallTest
    public void onUrlChanged_whenExistingCookieControlsBridge_shouldUpdateWebContents() {
        mMediator.setCookieControlsBridge(mCookieControlsBridge);
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        verify(mCookieControlsBridge, times(0)).updateWebContents(any(), any());

        mMediator.onUrlChanged();

        verify(mCookieControlsBridge, times(1)).updateWebContents(any(), any());
    }

    @Test
    @SmallTest
    public void onUrlChanged_whenNotExistingCookieControlsBridge_shouldCreateNewBridge() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        Assert.assertEquals(mMediator.getCookieControlsBridge(), null);

        mMediator.onUrlChanged();

        Assert.assertNotEquals(mMediator.getCookieControlsBridge(), null);
    }

    private String getIconIdentifierForTesting() {
        return mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconIdentifierForTesting();
    }

    /**
     * @param isIncognito Whether the current page is in an incognito mode.
     */
    private void setupStoreIconForTesting(boolean isIncognito) {
        doReturn(JUnitTestGURLs.BLUE_1).when(mLocationBarDataProvider).getCurrentGurl();
        doReturn(isIncognito).when(mLocationBarDataProvider).isIncognito();
    }
}
