// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.omnibox.status.StatusMediator.COOKIE_CONTROLS_ICON;

import android.content.Context;
import android.content.res.Resources;
import android.view.ContextThemeWrapper;
import android.view.View.OnClickListener;

import androidx.test.filters.SmallTest;

import com.google.android.material.color.MaterialColors;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.SearchEngineUtils;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator.PageInfoAction;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusView.IconTransitionType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsBridgeJni;
import org.chromium.components.content_settings.CookieControlsState;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link StatusMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class StatusMediatorUnitTest {
    private static final String TAG = "StatusMediatorUnitTest";
    private static final int CURRENT_TAB_ID = 5;
    private static final int NEW_TAB_ID = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NewTabPageDelegate mNewTabPageDelegate;
    @Mock private LocationBarDataProvider mLocationBarDataProvider;
    @Mock private FuseboxSessionState mFuseboxSessionState;
    @Mock private AutocompleteInput mAutocompleteInput;
    @Mock private SearchEngineUtils mSearchEngineUtils;
    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private PermissionDialogController mPermissionDialogController;
    @Mock private PageInfoIphController mPageInfoIphController;
    @Mock private CookieControlsBridge mCookieControlsBridge;
    @Mock private CookieControlsBridge.Natives mCookieControlsBridgeJniMock;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock UserPrefsJni mMockUserPrefsJni;
    @Mock private PrefService mPrefs;
    @Mock private Tracker mTracker;
    @Mock private OnClickListener mOnClickListener;
    @Mock private PageInfoAction mPageInfoAction;
    @Mock private Runnable mTogglePopupCallback;
    @Mock private Runnable mOnStatusViewHiddenForPageInfoRemoval;

    @Captor private ArgumentCaptor<PermissionDialogController.Observer> mPermissionObserverCaptor;

    private Context mContext;
    private PropertyModel mModel;
    private StatusMediator mMediator;
    private WindowAndroid mWindowAndroid;

    private final OneshotSupplierImpl<TemplateUrlService> mTemplateUrlServiceSupplier =
            new OneshotSupplierImpl<>();
    private final SettableNonNullObservableSupplier<Integer> mFuseboxStateSupplier =
            ObservableSuppliers.createNonNull(FuseboxState.DISABLED);
    private final SettableNullableObservableSupplier<GURL> mExactMatchUrlSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNonNullObservableSupplier<Integer> mRequestTypeSupplier =
            ObservableSuppliers.createNonNull(AutocompleteRequestType.SEARCH);

    @Before
    public void setUp() {
        SearchEngineUtils.setInstanceForTesting(mSearchEngineUtils);
        TrackerFactory.setTrackerForTests(mTracker);
        CookieControlsBridgeJni.setInstanceForTesting(mCookieControlsBridgeJniMock);
        UserPrefsJni.setInstanceForTesting(mMockUserPrefsJni);
        doReturn(mPrefs).when(mMockUserPrefsJni).get(mProfile);
        doReturn(false).when(mLocationBarDataProvider).isIncognito();
        doReturn(mNewTabPageDelegate).when(mLocationBarDataProvider).getNewTabPageDelegate();
        doReturn(mAutocompleteInput).when(mFuseboxSessionState).getAutocompleteInput();
        doReturn(mRequestTypeSupplier).when(mAutocompleteInput).getRequestTypeSupplier();

        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mWindowAndroid = new WindowAndroid(mContext, /* occlusionTrackingAllowed= */ false);
        mModel = new PropertyModel(StatusProperties.ALL_KEYS);
        mMediator =
                new StatusMediator(
                        mModel,
                        mContext,
                        mLocationBarDataProvider,
                        mPermissionDialogController,
                        mTemplateUrlServiceSupplier,
                        ObservableSuppliers.createNonNull(mProfile),
                        mPageInfoIphController,
                        mWindowAndroid,
                        mPageInfoAction,
                        mFuseboxStateSupplier,
                        mTogglePopupCallback,
                        mExactMatchUrlSupplier);
        mTemplateUrlServiceSupplier.set(mTemplateUrlService);

        StatusIconResource logo = new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0);
        mMediator.onSearchEngineIconChanged(logo);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    @Test
    @SmallTest
    public void testPermissionIconShown() {
        verify(mPermissionDialogController).addObserver(mPermissionObserverCaptor.capture());
        PermissionDialogController.Observer observer = mPermissionObserverCaptor.getValue();

        observer.onDialogResult(
                mWindowAndroid,
                new int[] {ContentSettingsType.MEDIASTREAM_CAMERA},
                ContentSetting.ALLOW);

        StatusIconResource icon = mModel.get(StatusProperties.STATUS_ICON_RESOURCE);
        assertNotNull("Permission icon should be shown", icon);
        assertNotNull(mModel.get(StatusProperties.STATUS_CLICK_LISTENER));
        assertEquals(IconTransitionType.ROTATE, icon.getTransitionType());
        icon.getAnimationFinishedCallback().run();
        verify(mPageInfoIphController, times(1))
                .onPermissionDialogShown(
                        any(),
                        eq(mMediator.getPermissionStatusHandlerForTesting().getIphTimeoutMs()));
    }

    @Test
    @SmallTest
    public void searchEngineLogo_isGoogleLogo() {
        mMediator.beginInput(mFuseboxSessionState);
        assertEquals(
                R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconRes());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_contextualTasksFusebox_evenWhenNtp() {
        doReturn(PageClassification.CO_BROWSING_COMPOSEBOX_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(/* prefetch= */ false);
        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();

        mMediator.beginInput(mFuseboxSessionState);

        // It should NOT show the status view at all (to avoid the gap).
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
    }

    @Test
    @SmallTest
    // OmniboxMobileParityUpdate is now always enabled
    public void searchEngineLogoPersistent() {
        doReturn(true).when(mNewTabPageDelegate).isCurrentlyVisible();

        mMediator.beginInput(mFuseboxSessionState);
        mMediator.endInput();
        assertTrue(mMediator.shouldDisplaySearchEngineIcon());

        doReturn(false).when(mNewTabPageDelegate).isCurrentlyVisible();

        mMediator.beginInput(mFuseboxSessionState);
        mMediator.endInput();
        assertFalse(mMediator.shouldDisplaySearchEngineIcon());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_onTextChanged_globeReplacesIconWhenTextIsSite() {
        mMediator.beginInput(mFuseboxSessionState);

        mExactMatchUrlSupplier.set(JUnitTestGURLs.BLUE_1);
        assertEquals(
                R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconRes());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_onTextChanged_noGlobeReplacementWhenUrlBarTextDoesNotMatch() {
        mMediator.beginInput(mFuseboxSessionState);

        mExactMatchUrlSupplier.set(null);
        assertNotEquals(
                R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconRes());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_onTextChanged_noGlobeReplacementWhenUrlBarTextIsEmpty() {
        mMediator.beginInput(mFuseboxSessionState);

        mExactMatchUrlSupplier.set(JUnitTestGURLs.BLUE_1);
        mExactMatchUrlSupplier.set(null);
        assertNotEquals(
                R.drawable.ic_globe_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconRes());
    }

    @Test
    @SmallTest
    public void searchEngineLogo_incognitoNoIcon() {
        doReturn(true).when(mLocationBarDataProvider).isIncognito();

        mMediator.endInput();
        mMediator.updateSecurityIcon(0, 0, 0);

        assertNull(mModel.get(StatusProperties.STATUS_ICON_RESOURCE));
    }

    @Test
    @SmallTest
    public void searchEngineLogo_maybeUpdateStatusIconForSearchEngineIconChanges() {
        mMediator.beginInput(mFuseboxSessionState);
        mMediator.updateSecurityIcon(0, 0, 0);

        assertTrue(mMediator.maybeUpdateStatusIconForSearchEngineIcon());
        assertEquals(
                R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconRes());
        assertNull(mModel.get(StatusProperties.STATUS_CLICK_LISTENER));
    }

    @Test
    @SmallTest
    public void testIncognitoStateChange() {
        doReturn(true).when(mLocationBarDataProvider).isIncognito();
        assertFalse(mModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));

        doReturn(true).when(mNewTabPageDelegate).isIncognitoNewTabPageCurrentlyVisible();
        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);

        assertEquals(
                R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconRes());
        assertFalse(mModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));

        mMediator.beginInput(mFuseboxSessionState);
        assertEquals(
                R.drawable.ic_logo_googleg_20dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconRes());
        assertFalse(mModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
    }

    @Test
    @SmallTest
    public void testStatusText() {
        mMediator.setUnfocusedLocationBarWidth(10);
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, true, true);
        // When both states, offline, and preview are enabled, paint preview has
        // the highest priority.
        assertEquals(
                "Incorrect text for paint preview status text",
                R.string.location_bar_paint_preview_page_status,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        assertEquals(
                "Incorrect color for paint preview status text",
                MaterialColors.getColor(mContext, R.attr.colorPrimary, TAG),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
        mMediator.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        assertEquals(
                "Incorrect color for paint preview status text",
                mContext.getColor(R.color.locationbar_status_preview_color_dark),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));

        assertNotNull(mModel.get(StatusProperties.STATUS_VIEW_BACKGROUND));

        // When only offline is enabled, it should be shown.
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, true, false);
        mMediator.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        assertEquals(
                "Incorrect text for offline page status text",
                R.string.location_bar_verbose_status_offline,
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        assertEquals(
                "Incorrect color for offline page status text",
                mContext.getColor(R.color.locationbar_status_offline_color_light),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
        mMediator.setBrandedColorScheme(BrandedColorScheme.LIGHT_BRANDED_THEME);
        assertEquals(
                "Incorrect color for offline page status text",
                mContext.getColor(R.color.locationbar_status_offline_color_dark),
                mModel.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR));
    }

    @Test
    @SmallTest
    public void testStatusIconAccessibility_hubSearch() {
        // Test default behaviour first.
        doReturn(PageClassification.NTP_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(/* prefetch= */ false);
        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);
        assertEquals(
                R.string.accessibility_toolbar_view_site_info,
                mModel.get(StatusProperties.STATUS_ACCESSIBILITY_DOUBLE_TAP_DESCRIPTION_RES));

        doReturn(PageClassification.ANDROID_HUB_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(/* prefetch= */ false);
        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);

        assertEquals(
                R.string.hub_search_status_view_back_button_icon_description,
                mModel.get(StatusProperties.STATUS_ICON_DESCRIPTION_RES));
        assertEquals(Resources.ID_NULL, mModel.get(StatusProperties.STATUS_VIEW_TOOLTIP_TEXT));
        assertNull(mModel.get(StatusProperties.STATUS_VIEW_BACKGROUND));
        assertEquals(
                R.string.accessibility_toolbar_exit_hub_search,
                mModel.get(StatusProperties.STATUS_ACCESSIBILITY_DOUBLE_TAP_DESCRIPTION_RES));
    }

    @Test
    @SmallTest
    @EnableFeatures(OmniboxFeatureList.EXACT_MATCH_FAVICONS)
    public void testStatusIcon_hubSearchWithExactMatchFaviconEnabled() {
        doReturn(PageClassification.ANDROID_HUB_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(/* prefetch= */ false);
        mExactMatchUrlSupplier.set(JUnitTestGURLs.BLUE_1);
        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);

        assertEquals(
                R.drawable.ic_arrow_back_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconRes());
    }

    @Test
    @SmallTest
    public void testSetTooltipText() {
        doReturn(PageClassification.NTP_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(/* prefetch= */ false);

        mMediator.setTooltipText(Resources.ID_NULL);
        // Assert that the below accessibility string is always set when #setTooltipText is called.
        assertEquals(
                R.string.accessibility_menu_info,
                mModel.get(StatusProperties.STATUS_VIEW_TOOLTIP_TEXT));
    }

    @Test
    @SmallTest
    public void testSetBackground() {
        doReturn(PageClassification.NTP_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(/* prefetch= */ false);

        mMediator.setBackground();
        // Assert that the non verbose drawable is always set when #setBackground is called.
        assertNotNull(mModel.get(StatusProperties.STATUS_VIEW_BACKGROUND));
    }

    @Test
    @SmallTest
    public void iphCookieControls_animatesOnHighlightCookieControl() {
        setupCookieControlsTest();

        assertNotEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());

        mMediator.onHighlightCookieControl(true);

        assertEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());

        mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback().run();
        verify(mPageInfoIphController, times(1)).showCookieControlsIph(anyInt(), anyInt());
        verify(mCookieControlsBridge, times(1)).onEntryPointAnimated();

        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);

        // CookieControlsIcon should not be set when no HIGH BreakageConfidenceLevel were
        // explicitly reported.
        mMediator.onHighlightCookieControl(false);
        assertNotEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());
    }

    @Test
    @SmallTest
    public void iphCookieControls() {
        setupCookieControlsTest();
        mMediator.onStatusChanged(
                CookieControlsState.BLOCKED3PC, /* enforcement= */ 0, /* expiration= */ 0);

        mMediator.onHighlightCookieControl(true);
        assertEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());
        mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getAnimationFinishedCallback().run();
        verify(mPageInfoIphController, times(1)).showCookieControlsIph(anyInt(), anyInt());
    }

    private void setupCookieControlsTest() {
        mMediator.beginInput(mFuseboxSessionState);
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, false);
        mMediator.setCookieControlsBridgeForTesting(mCookieControlsBridge);
        doReturn(true).when(mTracker).wouldTriggerHelpUi(any());
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
    }

    @Test
    @SmallTest
    public void cookieControlsIcon_doesNotAnimateIfWebContentsNull() {
        setupCookieControlsTest();

        doReturn(null).when(mTab).getWebContents();

        mMediator.onStatusChanged(
                CookieControlsState.BLOCKED3PC, /* enforcement= */ 0, /* expiration= */ 0);
        assertNotEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());

        mMediator.onHighlightCookieControl(true);

        // Cookie controls icon should NOT be shown.
        assertNotEquals(COOKIE_CONTROLS_ICON, getIconIdentifierForTesting());
        // IPH should NOT be shown.
        verify(mPageInfoIphController, never()).showCookieControlsIph(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void onUrlChanged_whenTabChanges_shouldUpdateWebContents() {
        mMediator.setCookieControlsBridgeForTesting(mCookieControlsBridge);
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        verify(mCookieControlsBridge, times(0)).updateWebContents(any(), any(), anyBoolean());

        doReturn(CURRENT_TAB_ID).when(mTab).getId();

        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(1)).updateWebContents(any(), any(), anyBoolean());

        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(1)).updateWebContents(any(), any(), anyBoolean());

        doReturn(NEW_TAB_ID).when(mTab).getId();
        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(2)).updateWebContents(any(), any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void onUrlChanged_whenTabNotChanging_shouldNotUpdateWebContents() {
        mMediator.setCookieControlsBridgeForTesting(mCookieControlsBridge);
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        doReturn(CURRENT_TAB_ID).when(mTab).getId();

        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(1)).updateWebContents(any(), any(), anyBoolean());

        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(1)).updateWebContents(any(), any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void onUrlChanged_whenTabCrashing_shouldUpdateWebContents() {
        mMediator.setCookieControlsBridgeForTesting(mCookieControlsBridge);
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        doReturn(CURRENT_TAB_ID).when(mTab).getId();

        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(1)).updateWebContents(any(), any(), anyBoolean());

        // Tab crashed, need to update the web contents at next url change.
        mMediator.onTabCrashed();
        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(2)).updateWebContents(any(), any(), anyBoolean());

        // Subsequent url changes on the same tab should not trigger any web contents update.
        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(2)).updateWebContents(any(), any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void onUrlChanged_whenNotExistingCookieControlsBridge_shouldCreateNewBridge() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();

        assertNull(mMediator.getCookieControlsBridgeForTesting());

        mMediator.onUrlChanged();

        assertNotEquals(null, mMediator.getCookieControlsBridgeForTesting());
    }

    @Test
    @SmallTest
    public void onUrlChanged_whenInIncognito_shouldUpdateWebContentsWithUpdatedIncognitoState() {
        mMediator.setCookieControlsBridgeForTesting(mCookieControlsBridge);
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(true).when(mProfile).isIncognitoBranded();
        doReturn(CURRENT_TAB_ID).when(mTab).getId();

        mMediator.onUrlChanged();
        verify(mCookieControlsBridge, times(1))
                .updateWebContents(any(), any(), /* isIncognitoBranded= */ eq(true));
    }

    @Test
    @SmallTest
    public void showStatusView_toggleVisibility() {
        mMediator.setShowStatusView(false);
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
        mMediator.setShowStatusView(true);
        assertTrue(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM})
    public void hideViewForSecureOrigins() {
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, false);
        assertTrue(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        mMediator.setShowStatusIconForSecureOrigins(false);
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        mMediator.updateSecurityIcon(R.drawable.ic_logo_googleg_20dp, 0, 0);
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.WARNING, false, false);
        assertTrue(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, false);
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        mMediator.setShowStatusIconForSecureOrigins(true);
        assertTrue(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM})
    public void testUpdateStatusViewVisibility() {
        // Focused URL should always show the status view.
        mMediator.beginInput(mFuseboxSessionState);
        assertTrue(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        mMediator.endInput();
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        // Non-secure pages should show the status view.
        mMediator.updateSecurityIcon(R.drawable.ic_logo_googleg_20dp, 0, 0);
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.DANGEROUS, false, false);
        assertTrue(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        // Secure pages should not show the status view if the flag is off.
        mMediator.setShowStatusIconForSecureOrigins(false);
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, false);
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
    }

    @Test
    @SmallTest
    public void testUpdateStatusViewVisibility_withPermissionIcon() {
        mMediator.setShowStatusIconForSecureOrigins(false);
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, false);
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        StatusProperties.PermissionIconResource icon =
                new StatusProperties.PermissionIconResource(null, false, "test_icon");
        mMediator.showPermissionIcon(icon);

        assertTrue(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
    }

    @Test
    @SmallTest
    public void testUpdateStatusViewVisibility_withVerboseStatusText() {
        mMediator.setShowStatusIconForSecureOrigins(false);
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, false);
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, true, false);

        assertTrue(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
    }

    @Test
    @SmallTest
    public void testUpdateStatusViewVisibility_withPaintPreview() {
        mMediator.setShowStatusIconForSecureOrigins(false);
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, false);
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        // Simulate Paint Preview active (third argument is true)
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, true);

        assertTrue(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM})
    public void setShowStatusIconForSecureOrigins_pageInfoMoved_phone() {
        // Set security level to SECURE, the status view should be hidden.
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, false);
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        // Try to show the status icon, it should not work because the page info is moved to app
        // menu.
        mMediator.setShowStatusIconForSecureOrigins(true);
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    @EnableFeatures({ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM})
    public void setShowStatusIconForSecureOrigins_pageInfoNotMoved_tablet() {
        // Tablet should not move page info to app menu, even if feature is enabled.

        // Set security level to SECURE, the status view should be shown initially.
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, false);
        assertTrue(mModel.get(StatusProperties.SHOW_STATUS_VIEW));

        // Try to hide the status icon, it should work because page info is not moved to app menu.
        mMediator.setShowStatusIconForSecureOrigins(false);
        assertFalse(mModel.get(StatusProperties.SHOW_STATUS_VIEW));
    }

    @Test
    @SmallTest
    public void testStatusClickListener_showPageInfo() {
        mMediator.endInput();
        doReturn(true).when(mLocationBarDataProvider).hasTab();
        doReturn(mTab).when(mLocationBarDataProvider).getTab();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(JUnitTestGURLs.BLUE_1).when(mLocationBarDataProvider).getCurrentGurl();
        mMediator.updateSecurityIcon(R.drawable.ic_globe_24dp, 0, 0);

        mModel.get(StatusProperties.STATUS_CLICK_LISTENER).onClick(/* view= */ null);
        verify(mPageInfoAction).show(any(), any());
    }

    @Test
    @SmallTest
    public void testStatusClickListener_withBackButtonPressListener() {
        doReturn(PageClassification.ANDROID_HUB_VALUE)
                .when(mLocationBarDataProvider)
                .getPageClassification(/* prefetch= */ false);
        mMediator.setOnStatusIconNavigateBackButtonPress(mOnClickListener);
        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);

        mModel.get(StatusProperties.STATUS_CLICK_LISTENER).onClick(/* view= */ null);
        verify(mOnClickListener).onClick(any());
    }

    @Test
    @SmallTest
    public void testStatusClickListener_whenUrlHasFocus() {
        mMediator.beginInput(mFuseboxSessionState);
        assertNull(mModel.get(StatusProperties.STATUS_CLICK_LISTENER));
    }

    @Test
    @SmallTest
    public void testFuseboxCompactMode_plusButton_allConditionsMet() {
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        mMediator.beginInput(mFuseboxSessionState);
        OmniboxFeatures.setIsDesktopPlatformForTesting(true);
        doReturn(AutocompleteRequestType.SEARCH).when(mAutocompleteInput).getRequestType();

        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);

        assertNotNull(mModel.get(StatusProperties.STATUS_ICON_RESOURCE));
        assertEquals(
                R.drawable.ic_add_round_20dp_with_inset,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconRes());

        assertNotNull(mModel.get(StatusProperties.STATUS_CLICK_LISTENER));
        mModel.get(StatusProperties.STATUS_CLICK_LISTENER).onClick(/* view= */ null);
        verify(mTogglePopupCallback).run();
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_PAGE_INFO_AS_APP_MENU_ITEM})
    public void testCallbackTriggeredWhenStatusViewHidden() {
        mMediator.setOnStatusViewHiddenForPageInfoRemoval(mOnStatusViewHiddenForPageInfoRemoval);

        mMediator.updateSecurityIcon(R.drawable.ic_logo_googleg_20dp, 0, 0);
        mMediator.updateVerboseStatus(ConnectionSecurityLevel.SECURE, false, false);
        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);

        verify(mOnStatusViewHiddenForPageInfoRemoval, times(3)).run();
    }

    @Test
    @SmallTest
    public void testFuseboxCompactMode_fallbackToSpark_notDesktop() {
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        mMediator.beginInput(mFuseboxSessionState);
        OmniboxFeatures.setIsDesktopPlatformForTesting(false);
        doReturn(AutocompleteRequestType.SEARCH).when(mAutocompleteInput).getRequestType();

        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);

        assertNotNull(mModel.get(StatusProperties.STATUS_ICON_RESOURCE));
        assertEquals(
                R.drawable.ic_add_round_20dp_with_inset,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconRes());
    }

    @Test
    @SmallTest
    public void testFuseboxCompactMode_fallbackToSpark_notConventional() {
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        mMediator.beginInput(mFuseboxSessionState);
        OmniboxFeatures.setIsDesktopPlatformForTesting(true);
        doReturn(AutocompleteRequestType.AI_MODE).when(mAutocompleteInput).getRequestType();

        mMediator.updateLocationBarIcon(IconTransitionType.CROSSFADE);

        assertNotNull(mModel.get(StatusProperties.STATUS_ICON_RESOURCE));
        assertEquals(
                R.drawable.search_spark_black_24dp,
                mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconRes());
    }

    private String getIconIdentifierForTesting() {
        return mModel.get(StatusProperties.STATUS_ICON_RESOURCE).getIconIdentifierForTesting();
    }
}
