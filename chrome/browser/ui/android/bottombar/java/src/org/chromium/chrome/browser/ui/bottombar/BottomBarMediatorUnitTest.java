// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link BottomBarMediator}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
public class BottomBarMediatorUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private BottomBarMediator.VisibilityDelegate mVisibilityDelegate;
    @Mock private Tab mTab;
    @Mock private Profile mProfile;
    @Mock private BottomBarButtonManager mButtonManager;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;
    @Mock private GlicKeyedService mGlicKeyedService;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<BottomBarButtonManager.Listener> mButtonManagerListenerCaptor;

    @Captor
    private ArgumentCaptor<GlicKeyedService.AllowedChangedObserver> mAllowedChangedObserverCaptor;

    private SettableNullableObservableSupplier<Profile> mProfileSupplier;

    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private SettableNonNullObservableSupplier<Boolean> mHomepageEnabledSupplier;
    private SettableNonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private PropertyModel mModel;
    private @Nullable BottomBarMediator mMediator;

    @Before
    public void setUp() {
        mTabSupplier = ObservableSuppliers.createNullable();
        mHomepageEnabledSupplier = ObservableSuppliers.createNonNull(false);
        mOmniboxFocusStateSupplier = ObservableSuppliers.createNonNull(false);
        mProfileSupplier = ObservableSuppliers.createNullable();
        mProfileSupplier.set(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mModel = new PropertyModel(BottomBarProperties.ALL_KEYS);
        when(mThemeColorProvider.getBrandedColorScheme())
                .thenReturn(BrandedColorScheme.APP_DEFAULT);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        GlicKeyedServiceFactory.setForTesting(mGlicKeyedService);
    }

    @After
    public void tearDown() {
        if (mMediator != null) {
            mMediator.destroy();
        }
    }

    @Test
    public void testInitialization_WithoutHomeButton_DoesNotObserveHomepage() {
        createMediator(/* shouldIncludeHomeButton= */ false);

        mHomepageEnabledSupplier.set(true);
        verify(mButtonManager, never()).setButtonVisibility(ActionId.HOME_BUTTON, true);
    }

    private void setupTab(GURL url, boolean isIncognito) {
        when(mTab.getUrl()).thenReturn(url);
        when(mTab.isOffTheRecord()).thenReturn(isIncognito);
        mTabSupplier.set(mTab);
    }

    @Test
    public void testConstructor() {
        createMediator(/* shouldIncludeHomeButton= */ true);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    public void testTabObserverCleanup_OnTabRemoved() {
        setupTab(JUnitTestGURLs.NTP_URL, false);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());

        mTabSupplier.set(null);
        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
    }

    @Test
    public void testVisibilityChange_EmptyUrl() {
        setupTab(GURL.emptyGURL(), false);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
    }

    @Test
    public void testVisibilityChange_Ntp_Incognito() {
        setupTab(JUnitTestGURLs.NTP_URL, true);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    public void testVisibilityChange_NotNtp() {
        setupTab(JUnitTestGURLs.EXAMPLE_URL, false);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false"})
    public void testVisibilityChange_DisableOnNtpDisabled_NtpTab() {
        setupTab(JUnitTestGURLs.NTP_URL, false);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"})
    public void testVisibilityChange_NtpToNonNtp() {
        setupTab(JUnitTestGURLs.NTP_URL, false);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        assertFalse(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(false);

        // Switch from NTP to Non-NTP
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"})
    public void testVisibilityChange_NonNtpToNtp() {
        setupTab(JUnitTestGURLs.EXAMPLE_URL, false);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        // Switch from Non-NTP to NTP
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertFalse(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(false);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false"})
    public void testVisibilityChange_DisableOnNtpDisabled_NtpToNonNtp() {
        setupTab(JUnitTestGURLs.NTP_URL, false);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        // Switch from NTP to Non-NTP
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false"})
    public void testVisibilityChange_DisableOnNtpDisabled_NonNtpToNtp() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(false);

        mTabSupplier.set(mTab);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        // Switch from Non-NTP to NTP
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    public void testHomeButtonVisibility_Enabled() {
        mHomepageEnabledSupplier.set(true);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mButtonManager).setButtonVisibility(ActionId.HOME_BUTTON, true);
    }

    @Test
    public void testHomeButtonVisibility_Disabled() {
        mHomepageEnabledSupplier.set(false);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mButtonManager).setButtonVisibility(ActionId.HOME_BUTTON, false);
    }

    @Test
    public void testHomeButtonVisibility_Toggle() {
        mHomepageEnabledSupplier.set(true);
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mButtonManager).setButtonVisibility(ActionId.HOME_BUTTON, true);

        mHomepageEnabledSupplier.set(false);
        verify(mButtonManager).setButtonVisibility(ActionId.HOME_BUTTON, false);

        mHomepageEnabledSupplier.set(true);
        verify(mButtonManager, times(2)).setButtonVisibility(ActionId.HOME_BUTTON, true);
    }

    @Test
    public void testTintChanged() {
        createMediator(/* shouldIncludeHomeButton= */ true);
        assertNotNull(mMediator);
        verify(mThemeColorProvider).addTintObserver(mMediator);

        mMediator.onTintChanged(null, null, BrandedColorScheme.INCOGNITO);
        assertTrue(mModel.get(BottomBarProperties.COLOR_SCHEME) == BrandedColorScheme.INCOGNITO);
        verify(mVisibilityDelegate).onBackgroundColorChanged();
    }

    @Test
    public void testGlicButtonVisibility_Disabled() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(false);
        mTabSupplier.set(mTab);

        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mButtonManager, atLeastOnce()).setButtonVisibility(ActionId.GLIC, false);
    }

    @Test
    public void testGlicButtonVisibility_ProfileDisabled() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(false);
        mTabSupplier.set(mTab);

        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, false);
    }

    @Test
    public void testGlicButtonVisibility_AllowedChanged() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(false);
        mTabSupplier.set(mTab);

        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mGlicKeyedService)
                .addAllowedChangedObserver(mAllowedChangedObserverCaptor.capture());
        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, false);

        // Simulate allowed state change
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        mAllowedChangedObserverCaptor.getValue().onAllowedStateChanged();

        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, true);
    }

    @Test
    public void testGlicButtonVisibility_Ntp() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.isOffTheRecord()).thenReturn(false);
        mTabSupplier.set(mTab);

        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mButtonManager, atLeastOnce()).setButtonVisibility(ActionId.GLIC, true);
    }

    @Test
    public void testGlicButtonVisibility_Incognito() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(true);
        mTabSupplier.set(mTab);

        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mButtonManager, atLeastOnce()).setButtonVisibility(ActionId.GLIC, true);
    }

    @Test
    public void testVisibilityChange_OmniboxFocus() {
        createMediator(/* shouldIncludeHomeButton= */ true);

        // Initially visible.
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        // Focus omnibox.
        mOmniboxFocusStateSupplier.set(true);
        assertFalse(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(false);

        // Unfocus omnibox.
        mOmniboxFocusStateSupplier.set(false);
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(2)).onVisibilityChanged(true);
    }

    @Test
    public void testUpdateNewTabButtonBackground_OnlyUpdatesModelOnStateChange() {
        createMediator(/* shouldIncludeHomeButton= */ true);

        verify(mButtonManager).setListener(mButtonManagerListenerCaptor.capture());
        BottomBarButtonManager.Listener listener = mButtonManagerListenerCaptor.getValue();

        when(mButtonManager.hasCenteredButton()).thenReturn(true);

        listener.onBottomBarStateChanged(/* visibilityChanged= */ true);
        assertTrue(mModel.get(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE));

        when(mButtonManager.hasCenteredButton()).thenReturn(false);
        listener.onBottomBarStateChanged(/* visibilityChanged= */ true);
        assertFalse(mModel.get(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE));
    }

    @Test
    public void testUpdateGlicVisibility_RecordsDecisionTime() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.BottomBar.GlicVisibilityDecisionTime")
                        .build();

        createMediator(/* shouldIncludeHomeButton= */ true);

        watcher.assertExpected();
    }

    @Test
    public void testUpdateGlicVisibility_RecordsTimeToAppear() {
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);

        createMediator(/* shouldIncludeHomeButton= */ true);

        // Bottom bar is visible by default in constructor.
        // Now make GLIC appear by enabling it for profile.
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord("Android.BottomBar.GlicTimeToAppearSinceBottomBarShown")
                        .build();

        // Trigger update by notifying allowed observer.
        verify(mGlicKeyedService)
                .addAllowedChangedObserver(mAllowedChangedObserverCaptor.capture());
        mAllowedChangedObserverCaptor.getValue().onAllowedStateChanged();

        watcher.assertExpected();

        // Now simulate a disappear and appear again. It should not record again.
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(false);
        mAllowedChangedObserverCaptor.getValue().onAllowedStateChanged();

        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        HistogramWatcher noRecordWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.BottomBar.GlicTimeToAppearSinceBottomBarShown")
                        .build();

        mAllowedChangedObserverCaptor.getValue().onAllowedStateChanged();

        noRecordWatcher.assertExpected();
    }

    private void createMediator(boolean shouldIncludeHomeButton) {
        mMediator =
                new BottomBarMediator(
                        mModel,
                        mButtonManager,
                        mThemeColorProvider,
                        mTabSupplier,
                        mHomepageEnabledSupplier,
                        mVisibilityDelegate,
                        shouldIncludeHomeButton,
                        mProfileSupplier,
                        mOmniboxFocusStateSupplier);
    }
}
