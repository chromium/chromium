// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
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

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<BottomBarButtonManager.Listener> mButtonManagerListenerCaptor;

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
        GlicEnabling.setEnabledForTesting(false);
    }

    @After
    public void tearDown() {
        if (mMediator != null) {
            mMediator.destroy();
        }
    }

    @Test
    public void testInitialization_WithoutHomeButton_DoesNotObserveHomepage() {
        createMediator(/* shouldIncludeHomeButton= */ false, /* shouldIncludeGlic= */ true);

        mHomepageEnabledSupplier.set(true);
        verify(mButtonManager, never()).setButtonVisibility(ActionId.HOME_BUTTON, true);
    }

    @Test
    public void testInitialization_WithoutGlic_DoesNotObserveProfile() {
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ false);

        mProfileSupplier.set(mProfile);
        verify(mGlicEnablingJniMock, never()).isEnabledForProfile(any());
    }

    private void setupTab(GURL url, boolean isIncognito) {
        when(mTab.getUrl()).thenReturn(url);
        when(mTab.isOffTheRecord()).thenReturn(isIncognito);
        mTabSupplier.set(mTab);
    }

    @Test
    public void testConstructor() {
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    public void testTabObserverCleanup_OnTabRemoved() {
        setupTab(JUnitTestGURLs.NTP_URL, false);
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());

        mTabSupplier.set(null);
        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
    }

    @Test
    public void testVisibilityChange_EmptyUrl() {
        setupTab(GURL.emptyGURL(), false);
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
    }

    @Test
    public void testVisibilityChange_Ntp_Incognito() {
        setupTab(JUnitTestGURLs.NTP_URL, true);
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    public void testVisibilityChange_NotNtp() {
        setupTab(JUnitTestGURLs.EXAMPLE_URL, false);
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

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
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

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
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

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
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

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
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

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
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

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
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

        verify(mButtonManager).setButtonVisibility(ActionId.HOME_BUTTON, true);
    }

    @Test
    public void testHomeButtonVisibility_Disabled() {
        mHomepageEnabledSupplier.set(false);
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

        verify(mButtonManager).setButtonVisibility(ActionId.HOME_BUTTON, false);
    }

    @Test
    public void testHomeButtonVisibility_Toggle() {
        mHomepageEnabledSupplier.set(true);
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

        verify(mButtonManager).setButtonVisibility(ActionId.HOME_BUTTON, true);

        mHomepageEnabledSupplier.set(false);
        verify(mButtonManager).setButtonVisibility(ActionId.HOME_BUTTON, false);

        mHomepageEnabledSupplier.set(true);
        verify(mButtonManager, times(2)).setButtonVisibility(ActionId.HOME_BUTTON, true);
    }

    @Test
    public void testTintChanged() {
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);
        assert mMediator != null;
        verify(mThemeColorProvider).addTintObserver(mMediator);

        mMediator.onTintChanged(null, null, BrandedColorScheme.INCOGNITO);
        assertTrue(mModel.get(BottomBarProperties.COLOR_SCHEME) == BrandedColorScheme.INCOGNITO);
    }

    @Test
    public void testGlicButtonVisibility_Disabled() {
        GlicEnabling.setEnabledForTesting(false);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(false);
        mTabSupplier.set(mTab);

        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, false);
    }

    @Test
    public void testGlicButtonVisibility_Ntp() {
        GlicEnabling.setEnabledForTesting(true);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.isOffTheRecord()).thenReturn(false);
        mTabSupplier.set(mTab);

        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, true);
    }

    @Test
    public void testGlicButtonVisibility_Incognito() {
        GlicEnabling.setEnabledForTesting(true);
        when(mGlicEnablingJniMock.isEnabledForProfile(any())).thenReturn(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isOffTheRecord()).thenReturn(true);
        mTabSupplier.set(mTab);

        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

        verify(mButtonManager).setButtonVisibility(ActionId.GLIC, true);
    }

    @Test
    public void testVisibilityChange_OmniboxFocus() {
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

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
        createMediator(/* shouldIncludeHomeButton= */ true, /* shouldIncludeGlic= */ true);

        verify(mButtonManager).setListener(mButtonManagerListenerCaptor.capture());
        BottomBarButtonManager.Listener listener = mButtonManagerListenerCaptor.getValue();

        when(mButtonManager.hasCenteredButton()).thenReturn(true);

        listener.onButtonChanged(true);
        assertTrue(mModel.get(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE));

        when(mButtonManager.hasCenteredButton()).thenReturn(false);
        listener.onButtonChanged(true);
        assertFalse(mModel.get(BottomBarProperties.IS_NEW_TAB_BACKGROUND_VISIBLE));
    }

    private void createMediator(boolean shouldIncludeHomeButton, boolean shouldIncludeGlic) {
        mMediator =
                new BottomBarMediator(
                        mModel,
                        mButtonManager,
                        mThemeColorProvider,
                        mTabSupplier,
                        mHomepageEnabledSupplier,
                        mVisibilityDelegate,
                        shouldIncludeHomeButton,
                        shouldIncludeGlic,
                        mProfileSupplier,
                        mOmniboxFocusStateSupplier);
    }
}
