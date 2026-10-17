// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.tips;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProviderSupplier;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.chrome.browser.tips.TipsSignalKeys;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment.HighlightedOption;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link BottomOmniboxTipsPromoHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomOmniboxTipsPromoHandlerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;

    private Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private BottomOmniboxTipsPromoHandler mHandler;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mWindowAndroid = new WindowAndroid(mActivity, /* occlusionTrackingAllowed= */ false);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        mHandler = new BottomOmniboxTipsPromoHandler();
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    @Test
    public void testGetPromoData() {
        FeatureTipPromoData data = mHandler.getPromoData(mActivity);

        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_positive_button_text),
                data.positiveButtonText);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_bottom_omnibox),
                data.mainPageTitle);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_description_bottom_omnibox),
                data.mainPageDescription);
        assertEquals(R.drawable.tips_promo_bottom_omnibox_logo, data.mainPageLogoViewRes);
        assertEquals(
                mActivity.getString(R.string.tips_promo_bottom_sheet_title_bottom_omnibox_short),
                data.detailPageTitle);
        assertEquals(
                List.of(
                        mActivity.getString(
                                R.string.tips_promo_bottom_sheet_first_step_bottom_omnibox),
                        mActivity.getString(
                                R.string.tips_promo_bottom_sheet_second_step_bottom_omnibox),
                        mActivity.getString(
                                R.string.tips_promo_bottom_sheet_third_step_bottom_omnibox)),
                data.detailPageSteps);
    }

    @Test
    public void testOnPromoAccepted_OpensAddressBarSettings() {
        mHandler.onPromoAccepted(mWindowAndroid);

        ArgumentCaptor<Bundle> argsCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mSettingsNavigation)
                .startSettings(
                        eq(mActivity), eq(AddressBarSettingsFragment.class), argsCaptor.capture());
        assertEquals(
                HighlightedOption.BOTTOM_TOOLBAR,
                argsCaptor.getValue().getInt(AddressBarSettingsFragment.HIGHLIGHTED_OPTION));
    }

    @Test
    public void testOnPromoAccepted_NoContext_DoesNothing() {
        WindowAndroid windowAndroid = mock(WindowAndroid.class);
        when(windowAndroid.getContext()).thenReturn(new WeakReference<>(null));

        mHandler.onPromoAccepted(windowAndroid);

        verifyNoInteractions(mSettingsNavigation);
    }

    @Test
    public void testGetCustomSignals_BottomControls() {
        attachControlsPosition(ControlsPosition.BOTTOM);
        assertBottomOmniboxStatus(1.0f);
    }

    @Test
    public void testGetCustomSignals_TopControls() {
        attachControlsPosition(ControlsPosition.TOP);
        assertBottomOmniboxStatus(0.0f);
    }

    @Test
    public void testGetCustomSignals_NoProvider_DefaultsToTop() {
        assertBottomOmniboxStatus(0.0f);
    }

    private void attachControlsPosition(@ControlsPosition int position) {
        when(mBrowserControlsStateProvider.getControlsPosition()).thenReturn(position);
        BrowserControlsStateProviderSupplier.attach(
                mWindowAndroid.getUnownedUserDataHost(),
                ObservableSuppliers.createMonotonic(mBrowserControlsStateProvider));
    }

    private void assertBottomOmniboxStatus(float expected) {
        assertEquals(
                Map.of(TipsSignalKeys.BOTTOM_OMNIBOX_STATUS, expected),
                mHandler.getCustomSignals(mWindowAndroid));
    }
}
