// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
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
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link BottomBarMediator}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarMediatorUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private BottomBarMediator.VisibilityDelegate mVisibilityDelegate;
    @Mock private Tab mTab;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private SettableNullableObservableSupplier<Tab> mTabSupplier;
    private PropertyModel mModel;
    private @Nullable BottomBarMediator mMediator;

    @Before
    public void setUp() {
        mTabSupplier = ObservableSuppliers.createNullable();
        mModel = new PropertyModel(BottomBarProperties.ALL_KEYS);
        when(mThemeColorProvider.getBrandedColorScheme())
                .thenReturn(BrandedColorScheme.APP_DEFAULT);
    }

    @After
    public void tearDown() {
        if (mMediator != null) {
            mMediator.destroy();
        }
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
    public void testConstructor() {
        mMediator =
                new BottomBarMediator(
                        mModel, mThemeColorProvider, mTabSupplier, mVisibilityDelegate);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
    public void testTabObserverCleanup_OnTabRemoved() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        mTabSupplier.set(mTab);
        mMediator =
                new BottomBarMediator(
                        mModel, mThemeColorProvider, mTabSupplier, mVisibilityDelegate);

        verify(mTab).addObserver(mTabObserverCaptor.capture());

        mTabSupplier.set(null);
        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
    public void testVisibilityChange_EmptyUrl() {
        when(mTab.getUrl()).thenReturn(GURL.emptyGURL());
        when(mTab.isIncognito()).thenReturn(false);

        mTabSupplier.set(mTab);
        mMediator =
                new BottomBarMediator(
                        mModel, mThemeColorProvider, mTabSupplier, mVisibilityDelegate);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
    public void testVisibilityChange_Ntp_Incognito() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.isIncognito()).thenReturn(true);

        mTabSupplier.set(mTab);
        mMediator =
                new BottomBarMediator(
                        mModel, mThemeColorProvider, mTabSupplier, mVisibilityDelegate);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
    public void testVisibilityChange_NotNtp() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(false);

        mTabSupplier.set(mTab);
        mMediator =
                new BottomBarMediator(
                        mModel, mThemeColorProvider, mTabSupplier, mVisibilityDelegate);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false"})
    public void testVisibilityChange_DisableOnNtpDisabled_NtpTab() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.isIncognito()).thenReturn(false);

        mTabSupplier.set(mTab);
        mMediator =
                new BottomBarMediator(
                        mModel, mThemeColorProvider, mTabSupplier, mVisibilityDelegate);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/true"})
    public void testVisibilityChange_NtpToNonNtp() {
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.isIncognito()).thenReturn(false);

        mTabSupplier.set(mTab);
        mMediator =
                new BottomBarMediator(
                        mModel, mThemeColorProvider, mTabSupplier, mVisibilityDelegate);

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
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        when(mTab.isIncognito()).thenReturn(false);

        mTabSupplier.set(mTab);
        mMediator =
                new BottomBarMediator(
                        mModel, mThemeColorProvider, mTabSupplier, mVisibilityDelegate);

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
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.isIncognito()).thenReturn(false);

        mTabSupplier.set(mTab);
        mMediator =
                new BottomBarMediator(
                        mModel, mThemeColorProvider, mTabSupplier, mVisibilityDelegate);

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
        when(mTab.isIncognito()).thenReturn(false);

        mTabSupplier.set(mTab);
        mMediator =
                new BottomBarMediator(
                        mModel, mThemeColorProvider, mTabSupplier, mVisibilityDelegate);

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);

        // Switch from Non-NTP to NTP
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        mTabObserverCaptor.getValue().onUrlUpdated(mTab);

        assertTrue(mModel.get(BottomBarProperties.IS_VISIBLE));
        verify(mVisibilityDelegate, times(1)).onVisibilityChanged(true);
    }
}
