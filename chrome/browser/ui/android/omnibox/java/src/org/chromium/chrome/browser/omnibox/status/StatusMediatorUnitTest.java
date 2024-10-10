// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
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
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustSignalsCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusView.IconTransitionType;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.content_settings.CookieBlocking3pcdStatus;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsBridgeJni;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link StatusMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class StatusMediatorUnitTest {
    private static final String TAG = "StatusMediatorUnitTest";
    private static final String TEST_SEARCH_URL = "https://www.test.com";

    public static final int CURRENT_TAB_ID = 5;
    public static final int NEW_TAB_ID = 1;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule JniMocker mJniMocker = new JniMocker();

    private @Mock NewTabPageDelegate mNewTabPageDelegate;
    private @Mock LocationBarDataProvider mLocationBarDataProvider;
    private @Mock UrlBarEditingTextStateProvider mUrlBarEditingTextStateProvider;
    private @Mock SearchEngineUtils mSearchEngineUtils;
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
    private @Mock StatusView mStatusView;
    @Mock UserPrefsJni mMockUserPrefsJni;
    @Mock private PrefService mPrefs;
    @Mock private Tracker mTracker;

    Context mContext;

    PropertyModel mModel;
    StatusMediator mMediator;
    OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier;
    WindowAndroid mWindowAndroid;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mWindowAndroid = new WindowAndroid(mContext);

        SearchEngineUtils.setInstanceForTesting(mSearchEngineUtils);

        mModel = new PropertyModel(StatusProperties.ALL_KEYS);

        mJniMocker.mock(CookieControlsBridgeJni.TEST_HOOKS, mCookieControlsBridgeJniMock);

        // By default return google g, but this behavior is overridden in some tests.
        var logo = new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0);

        doReturn(false).when(mLocationBarDataProvider).isIncognito();
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(logo).when(mSearchEngineUtils).getSearchEngineLogo(BrandedColorScheme.APP_DEFAULT);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefsJni);
        doReturn(mPrefs).when(mMockUserPrefsJni).get(mProfile);

        TrackerFactory.setTrackerForTests(mTracker);

        setupStatusMediator(/* isTablet= */ false);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
        TrackerFactory.setTrackerForTests(null);
    }

    private void setupStatusMediator(boolean isTablet) {
        mTemplateUrlServiceSupplier = new OneshotSupplierImpl<>();
        ObservableSupplierImpl<MerchantTrustSignalsCoordinator>
                merchantTrustSignalsCoordinatorObservableSupplier = new ObservableSupplierImpl<>();
        mMediator =
                new StatusMediator(
                        mModel,
                        mContext,
                        mUrlBarEditingTextStateProvider,
                        isTablet,
                        mLocationBarDataProvider,
                        mPermissionDialogController,
                        mTemplateUrlServiceSupplier,
                        () -> mProfile,
                        mPageInfoIPHController,
                        mWindowAndroid,
                        merchantTrustSignalsCoordinatorObservableSupplier);
        mTemplateUrlServiceSupplier.set(mTemplateUrlService);
        merchantTrustSignalsCoordinatorObservableSupplier.set(mMerchantTrustSignalsCoordinator);
    }

    @Test
    @SmallTest
    public void searchEngineLogo_isGoogleLogo() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        Assert.assertEquals(
                R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    public void testStatusViewHoverActions() {
        // Tooltip and hover highlight should be set when StatusViewIcon is visible.
        mMediator.setStatusIconShown(true);
        StatusViewBinder.applyStatusIconAndTooltipProperties(mModel, mStatusView);
        Assert.assertEquals(
                R.string.accessibility_menu_info,
                mModel.get(StatusProperties.STATUS_VIEW_TOOLTIP_TEXT));
        Assert.assertEquals(
                R.drawable.status_view_ripple,
                mModel.get(StatusProperties.STATUS_VIEW_HOVER_HIGHLIGHT));

        // Tooltip and hover highlight should NOT be set when StatusViewIcon is gone.
        mMediator.setStatusIconShown(false);
        StatusViewBinder.applyStatusIconAndTooltipProperties(mModel, mStatusView);
        Assert.assertEquals(
                Resources.ID_NULL, mModel.get(StatusProperties.STATUS_VIEW_TOOLTIP_TEXT));
        Assert.assertEquals(
                Resources.ID_NULL, mModel.get(StatusProperties.STATUS_VIEW_HOVER_HIGHLIGHT));
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

        doReturn(true).when(mLocationBarDataProvider).isIncognitoBranded();
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
        Assert.assertEquals(
                R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenTextIsSite() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();

        mMediator.updateLocationBarIconForDefaultMatchCategory(false);
        Assert.assertEquals(
                R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenAutocompleteSiteContainsText() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();

        mMediator.updateLocationBarIconForDefaultMatchCategory(false);
        Assert.assertEquals(
                R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconResForTesting());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_onTextChanged_noGlobeReplacementWhenUrlBarTextDoesNotMatch() {
        mMediator.setUrlHasFocus(true);
        mMediator.setShowIconsWhenUrlFocused(true);
        doReturn(TEST_SEARCH_URL).when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();

        mMediator.updateLocationBarIconForDefaultMatchCategory(true);
        Assert.assertNotEquals(
                R.drawable.ic_globe_24dp,
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
        Assert.assertNotEquals(
                R.drawable.ic_globe_24dp,
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
        Assert.assertEquals(
                R.drawable.ic_logo_googleg_20dp,
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

        verify(mSearchEngineUtils, times(1)).getSearchEngineLogo(BrandedColorScheme.APP_DEFAULT);

        mMediator.setUrlFocusChangePercent(0.0f);
        Assert.assertEquals(false, mModel.get(StatusProperties.SHOW_STATUS_ICON));
    }

    @Test
    @SmallTest
    public void resolveUrlBarTextWithAutocomplete_urlBarTextEmpty() {
        Assert.assertEquals(
                "Empty urlBarText should resolve to empty urlBarTextWithAutocomplete",
                "",
                mMediator.resolveUrlBarTextWithAutocomplete(""));
    }

    @Test
    @SmallTest
    public void resolveUrlBarTextWithAutocomplete_urlBarTextMismatchesAutocompleteText() {
        doReturn("https://foo.com").when(mUrlBarEditingTextStateProvider).getTextWithAutocomplete();
        String msg =
                "The urlBarText should only resolve to the autocomplete text if it's a "
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
        Assert.assertEquals(
                "Incorrect text for paint preview status text",
                R.string.location_bar_paint_preview_page_status,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        Assert.assertEquals(
                "Incorrect color for paint preview status text",
                MaterialColors.getColor(mContext, R.attr.colorPrimary, TAG),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
        mMediator.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        Assert.assertEquals(
                "Incorrect color for paint preview status text",
                mContext.getColor(R.color.locationbar_status_preview_color_dark),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));

        // When only offline is enabled, it should be shown.
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, true, false);
        mMediator.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        Assert.assertEquals(
                "Incorrect text for offline page status text",
                R.string.location_bar_verbose_status_offline,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        Assert.assertEquals(
                "Incorrect color for offline page status text",
                mContext.getColor(R.color.locationbar_status_offline_color_light),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
        mMediator.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        Assert.assertEquals(
                "Incorrect color for offline page status text",
                mContext.getColor(R.color.locationbar_status_offline_color_dark),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
    }

    @Test
    @SmallTest
    public void testTemplateUrlServiceChanged() {
        mMediator.setShowIconsWhenUrlFocused(true);
        mMediator.setUrlHasFocus(true);

        mMediator.onTemplateURLServiceChanged();
        verify(mSearchEngineUtils, times(2)).getSearchEngineLogo(BrandedColorScheme.APP_DEFAULT);
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
        Assert.assertEquals(
                IconTransitionType.ROTATE,
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
        Assert.assertEquals(
                IconTransitionType.ROTATE,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getTransitionType());
        Assert.assertNotNull(
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback());
        mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback().run();
        verify(mPageInfoIPHController, times(0)).showStoreIconIPH(anyInt(), eq(0));
    }

    @Test
    @SmallTest
    public void iphCookieControls_animatesonHighlightCookieControl() {
        setupCookieControlsTest();

        Assert.assertNotEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());

        mMediator.onHighlightCookieControl(true);

        Assert.assertEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());

        mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback().run();
        verify(mPageInfoIPHController, times(1)).showCookieControlsIPH(anyInt(), anyInt());
        verify(mCookieControlsBridge, times(1)).onEntryPointAnimated();

        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);

        // CookieControlsIcon should not be set when no HIGH BreakageConfidenceLevel were
        // explicitly reported.
        mMediator.onHighlightCookieControl(false);
        Assert.assertNotEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());
    }

    @Test
    @SmallTest
    public void iphCookieControls_showIPHOnlyWhenNotIn3pcd() {
        setupCookieControlsTest();
        mMediator.onStatusChanged(
                /* controls_visible= */ true,
                /* protections_on= */ true,
                /* enforcement= */ 0,
                CookieBlocking3pcdStatus.NOT_IN3PCD,
                /* expiration= */ 0);

        mMediator.onHighlightCookieControl(true);
        Assert.assertEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());
        mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback().run();
        // Not in 3PCD, IPH is shown.
        verify(mPageInfoIPHController, times(1)).showCookieControlsIPH(anyInt(), anyInt());

        mMediator.onStatusChanged(
                /* controls_visible= */ true,
                /* protections_on= */ true,
                /* enforcement= */ 0,
                CookieBlocking3pcdStatus.LIMITED,
                /* expiration= */ 0);

        mMediator.onHighlightCookieControl(true);
        Assert.assertEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());
        // Limited 3PCD, IPH is NOT shown.
        verify(mPageInfoIPHController, times(1)).showCookieControlsIPH(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void iphCookieControls_onboardingNoticeNotYetAcked() {
        setupCookieControlsTest();

        // No interaction with the Tracking Protection onboarding notice yet.
        doReturn(0).when(mPrefs).getInteger(Pref.TRACKING_PROTECTION_ONBOARDING_ACK_ACTION);

        mMediator.onHighlightCookieControl(true);
        mMediator.onPageLoadStopped();
        Assert.assertEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());

        mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback().run();
        verify(mPageInfoIPHController, times(1)).showCookieControlsIPH(anyInt(), anyInt());
        verify(mCookieControlsBridge, times(1)).onEntryPointAnimated();
    }

    private void setupCookieControlsTest() {
        mMediator.setUrlHasFocus(true);
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, false);
        mMediator.setCookieControlsBridge(mCookieControlsBridge);
        doReturn(2).when(mPrefs).getInteger(Pref.TRACKING_PROTECTION_ONBOARDING_ACK_ACTION);
        doReturn(true).when(mTracker).wouldTriggerHelpUI(any());
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
    }

    @Test
    @SmallTest
    public void cookieControlsIcon_doesNotAnimateIfWebContentsNull() {
        setupCookieControlsTest();

        doReturn(null).when(mTab).getWebContents();

        mMediator.onStatusChanged(
                /* controls_visible= */ true,
                /* protections_on= */ true,
                /* enforcement= */ 0,
                CookieBlocking3pcdStatus.LIMITED,
                /* expiration= */ 0);
        Assert.assertNotEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());

        mMediator.onPageLoadStopped();

        // Cookie controls icon should NOT be shown.
        Assert.assertNotEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());
        // IPH should NOT be shown.
        verify(mPageInfoIPHController, never()).showCookieControlsIPH(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void onUrlChanged_whenTabChanges_shouldUpdateWebContents() {
        mMediator.setCookieControlsBridge(mCookieControlsBridge);
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        verify(mCookieControlsBridge, times(0)).updateWebContents(any(), any());

        doReturn(CURRENT_TAB_ID).when(mTab).getId();

        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(1)).updateWebContents(any(), any());

        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(1)).updateWebContents(any(), any());

        doReturn(NEW_TAB_ID).when(mTab).getId();
        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(2)).updateWebContents(any(), any());
    }

    @Test
    @SmallTest
    public void onUrlChanged_whenTabNotChanging_shouldNotUpdateWebContents() {
        mMediator.setCookieControlsBridge(mCookieControlsBridge);
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        doReturn(CURRENT_TAB_ID).when(mTab).getId();

        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(1)).updateWebContents(any(), any());

        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(1)).updateWebContents(any(), any());
    }

    @Test
    @SmallTest
    public void onUrlChanged_whenTabCrashing_shouldUpdateWebContents() {
        mMediator.setCookieControlsBridge(mCookieControlsBridge);
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        doReturn(CURRENT_TAB_ID).when(mTab).getId();

        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(1)).updateWebContents(any(), any());

        // Tab crashed, need to update the web contents at next url change.
        mMediator.onTabCrashed();
        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(2)).updateWebContents(any(), any());

        // Subsequent url changes on the same tab should not trigger any web contents update.
        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(2)).updateWebContents(any(), any());
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

    @Test
    @SmallTest
    public void showStatusView_toggleVisibility() {
        mMediator.setShowStatusView(false);
        Assert.assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
        mMediator.setShowStatusView(true);
        Assert.assertTrue(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
    }

    private String getIconIdentifierForTesting() {
        return mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconIdentifierForTesting();
    }

    /**
     * @param isIncognito Whether the current page is in an incognito mode.
     */
    private void setupStoreIconForTesting(boolean isIncognito) {
        doReturn(JUnitTestGURLs.BLUE_1).when(mLocationBarDataProvider).getCurrentGurl();
        doReturn(isIncognito).when(mLocationBarDataProvider).isIncognitoBranded();
        doReturn(isIncognito).when(mLocationBarDataProvider).isOffTheRecord();
    }
}
