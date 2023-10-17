// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType.AUTOFILL_PROFILE_SCREEN;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType.CREDIT_CARD_SCREEN;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType.HOME_SCREEN;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode.DISABLED;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode.WRAP_CONTENT;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the `FastCheckoutSheetContent` class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FastCheckoutSheetContentTest {
    private static final double DELTA = 0.001;
    private static final int HEADER_HEIGHT = 10;
    private static final int PROFILE_HEIGHT = 15;
    private static final int CREDIT_CARD_HEIGHT = 10;
    private static final int CONTENT_VIEW_HEIGHT = 40;
    private static final int CONTAINER_HEIGHT = 100;

    private FastCheckoutSheetContent mSheetContent;
    @Mock private View mContentView;
    @Mock private ViewGroup mContentViewParent;
    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private FastCheckoutSheetState mState;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mSheetContent = new FastCheckoutSheetContent(mState, mContentView);

        when(mResources.getDimensionPixelSize(R.dimen.fast_checkout_detail_sheet_header_height))
                .thenReturn(HEADER_HEIGHT);
        when(mResources.getDimensionPixelSize(
                        R.dimen.fast_checkout_detail_sheet_height_single_address))
                .thenReturn(PROFILE_HEIGHT);
        when(mResources.getDimensionPixelSize(
                        R.dimen.fast_checkout_detail_sheet_height_single_credit_card))
                .thenReturn(CREDIT_CARD_HEIGHT);
        when(mContext.getResources()).thenReturn(mResources);
        when(mContentView.getContext()).thenReturn(mContext);
        when(mContentView.getParent()).thenReturn(mContentViewParent);
        when(mContentView.getMeasuredHeight()).thenReturn(CONTENT_VIEW_HEIGHT);
        when(mState.getContainerHeight()).thenReturn(CONTAINER_HEIGHT);
    }

    @Test
    public void getFullHeightRatio_HomeScreen_ReturnsWrapContent() {
        when(mState.getCurrentScreen()).thenReturn(HOME_SCREEN);
        assertEquals(WRAP_CONTENT, mSheetContent.getFullHeightRatio(), DELTA);
    }

    @Test
    public void getHalfHeightRatio_HomeScreen_ReturnsDisabled() {
        when(mState.getCurrentScreen()).thenReturn(HOME_SCREEN);
        assertEquals(DISABLED, mSheetContent.getHalfHeightRatio(), DELTA);
    }

    @Test
    public void
            getFullHeightRatio_ProfileScreenAboveThreshold_ReturnsBottomsheetHeightByContainerHeight() {
        when(mState.getCurrentScreen()).thenReturn(AUTOFILL_PROFILE_SCREEN);
        when(mState.getNumOfAutofillProfiles()).thenReturn(3);
        assertEquals(0.4f, mSheetContent.getFullHeightRatio(), DELTA);
    }

    @Test
    public void getFullHeightRatio_ProfileScreenBelowThreshold_ReturnsWrapContent() {
        when(mState.getCurrentScreen()).thenReturn(AUTOFILL_PROFILE_SCREEN);
        when(mState.getNumOfAutofillProfiles()).thenReturn(2);
        assertEquals(WRAP_CONTENT, mSheetContent.getFullHeightRatio(), DELTA);
    }

    @Test
    public void
            getHalfHeightRatio_ProfileScreenAboveThreshold_ReturnsDesiredHeightByContainerHeight() {
        when(mState.getCurrentScreen()).thenReturn(AUTOFILL_PROFILE_SCREEN);
        when(mState.getNumOfAutofillProfiles()).thenReturn(3);
        assertEquals(0.48f, mSheetContent.getHalfHeightRatio(), DELTA);
    }

    @Test
    public void getHalfHeightRatio_ProfileScreenBelowThreshold_ReturnsDisabled() {
        when(mState.getCurrentScreen()).thenReturn(AUTOFILL_PROFILE_SCREEN);
        when(mState.getNumOfAutofillProfiles()).thenReturn(2);
        assertEquals(DISABLED, mSheetContent.getHalfHeightRatio(), DELTA);
    }

    @Test
    public void
            getFullHeightRatio_CreditCardScreenAboveThreshold_ReturnsBottomsheetHeightByContainerHeight() {
        when(mState.getCurrentScreen()).thenReturn(CREDIT_CARD_SCREEN);
        when(mState.getNumOfCreditCards()).thenReturn(4);
        assertEquals(0.4f, mSheetContent.getFullHeightRatio(), DELTA);
    }

    @Test
    public void getFullHeightRatio_CreditCardScreenBelowThreshold_ReturnsWrapContent() {
        when(mState.getCurrentScreen()).thenReturn(CREDIT_CARD_SCREEN);
        when(mState.getNumOfCreditCards()).thenReturn(3);
        assertEquals(WRAP_CONTENT, mSheetContent.getFullHeightRatio(), DELTA);
    }

    @Test
    public void
            getHalfHeightRatio_CreditCardScreenAboveThreshold_ReturnsDesiredHeightByContainerHeight() {
        when(mState.getCurrentScreen()).thenReturn(CREDIT_CARD_SCREEN);
        when(mState.getNumOfCreditCards()).thenReturn(4);
        assertEquals(0.45f, mSheetContent.getHalfHeightRatio(), DELTA);
    }

    @Test
    public void getHalfHeightRatio_CreditCardScreenBelowThreshold_ReturnsDisabled() {
        when(mState.getCurrentScreen()).thenReturn(CREDIT_CARD_SCREEN);
        when(mState.getNumOfCreditCards()).thenReturn(3);
        assertEquals(DISABLED, mSheetContent.getHalfHeightRatio(), DELTA);
    }
}
