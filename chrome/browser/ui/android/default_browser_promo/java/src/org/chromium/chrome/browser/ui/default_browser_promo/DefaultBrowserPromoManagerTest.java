// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.role.RoleManager;
import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserPromoEntryPoint;
import org.chromium.chrome.browser.util.DefaultBrowserInfo.DefaultBrowserState;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

/** Test whether metrics are correctly recorded by {@link DefaultBrowserPromoManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DefaultBrowserPromoManagerTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock WindowAndroid mWindowAndroid;
    @Mock Activity mActivity;
    @Mock RoleManager mRoleManager;
    @Mock Intent mIntent;
    @Mock DefaultBrowserPromoImpressionCounter mImpressionCounter;
    @Mock DefaultBrowserStateProvider mStateProvider;

    @Before
    public void setup() {
        doReturn(mRoleManager).when(mActivity).getSystemService(Context.ROLE_SERVICE);
        doReturn(mIntent).when(mRoleManager).createRequestRoleIntent(RoleManager.ROLE_BROWSER);
    }

    @Test
    public void testRecordWhenNoDefault_OutcomeNoDefault() {
        testRecord(DefaultBrowserState.NO_DEFAULT, DefaultBrowserState.NO_DEFAULT);
    }

    @Test
    public void testRecordWhenNoDefault_OutcomeOtherDefault() {
        testRecord(DefaultBrowserState.NO_DEFAULT, DefaultBrowserState.OTHER_DEFAULT);
    }

    @Test
    public void testRecordWhenNoDefault_OutcomeChromeDefault() {
        testRecord(DefaultBrowserState.NO_DEFAULT, DefaultBrowserState.CHROME_DEFAULT);
    }

    @Test
    public void testRecordWhenOtherDefault_OutComeNoDefault() {
        testRecord(DefaultBrowserState.OTHER_DEFAULT, DefaultBrowserState.NO_DEFAULT);
    }

    @Test
    public void testRecordWhenOtherDefault_OutComeOtherDefault() {
        testRecord(DefaultBrowserState.OTHER_DEFAULT, DefaultBrowserState.OTHER_DEFAULT);
    }

    @Test
    public void testRecordWhenOtherDefault_OutComeChromeDefault() {
        testRecord(DefaultBrowserState.OTHER_DEFAULT, DefaultBrowserState.CHROME_DEFAULT);
    }

    @Test
    public void testRecordWhenNoDefault_OutcomeChromeDefault_FirstPromo() {
        when(mImpressionCounter.getPromoCount()).thenReturn(1);
        testRecord(
                DefaultBrowserState.OTHER_DEFAULT,
                DefaultBrowserState.CHROME_DEFAULT,
                "Android.DefaultBrowserPromo.Outcome.OtherDefault.FirstPromo");
    }

    @Test
    public void testRecordWhenNoDefault_OutcomeChromeDefault_SecondPromo() {
        when(mImpressionCounter.getPromoCount()).thenReturn(2);
        testRecord(
                DefaultBrowserState.OTHER_DEFAULT,
                DefaultBrowserState.CHROME_DEFAULT,
                "Android.DefaultBrowserPromo.Outcome.OtherDefault.SecondPromo");
    }

    @Test
    public void testRecordWhenNoDefault_OutcomeChromeDefault_ThirdPromo() {
        when(mImpressionCounter.getPromoCount()).thenReturn(3);
        testRecord(
                DefaultBrowserState.OTHER_DEFAULT,
                DefaultBrowserState.CHROME_DEFAULT,
                "Android.DefaultBrowserPromo.Outcome.OtherDefault.ThirdPromo");
    }

    @Test
    public void testRecordWhenNoDefault_OutcomeChromeDefault_FourthPromo() {
        when(mImpressionCounter.getPromoCount()).thenReturn(4);
        testRecord(
                DefaultBrowserState.OTHER_DEFAULT,
                DefaultBrowserState.CHROME_DEFAULT,
                "Android.DefaultBrowserPromo.Outcome.OtherDefault.FourthPromo");
    }

    @Test
    public void testRecordWhenNoDefault_OutcomeChromeDefault_SixthPromo() {
        when(mImpressionCounter.getPromoCount()).thenReturn(6);
        testRecord(
                DefaultBrowserState.OTHER_DEFAULT,
                DefaultBrowserState.CHROME_DEFAULT,
                "Android.DefaultBrowserPromo.Outcome.OtherDefault.FifthOrMorePromo");
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.DEFAULT_BROWSER_PROMO_ENTRY_POINT + ":show_app_menu_item/true"
    })
    public void testRecordOutcomeWithSource() {
        // Create a Manager with a specific source ("AppMenu").
        var manager =
                new DefaultBrowserPromoManager(
                        mActivity,
                        mWindowAndroid,
                        mImpressionCounter,
                        mStateProvider,
                        DefaultBrowserPromoEntryPoint.APP_MENU);

        int currentState = DefaultBrowserState.OTHER_DEFAULT;
        int outcomeState = DefaultBrowserState.CHROME_DEFAULT;

        when(mStateProvider.getCurrentDefaultBrowserState()).thenReturn(currentState, outcomeState);

        // Just return a dummy integer.
        doReturn(1).when(mWindowAndroid).showCancelableIntent(any(Intent.class), any(), any());

        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DefaultBrowserPromo.Outcome.AppMenu", outcomeState)
                        .build();

        // Trigger the promo.
        manager.promoByRoleManager();

        // 6. Capture the callback.
        ArgumentCaptor<IntentCallback> onShowCallbackCaptor =
                ArgumentCaptor.forClass(IntentCallback.class);
        verify(mWindowAndroid)
                .showCancelableIntent(eq(mIntent), onShowCallbackCaptor.capture(), any());

        // Trigger the callback. We close the dialog and the histogram records the outcome.
        onShowCallbackCaptor.getValue().onIntentCompleted(1, null);

        histogram.assertExpected("AppMenu specific outcome should be recorded");
    }

    private void testRecord(
            @DefaultBrowserState int currentState, @DefaultBrowserState int outcomeState) {
        testRecord(currentState, outcomeState, null);
    }

    private void testRecord(
            @DefaultBrowserState int currentState,
            @DefaultBrowserState int outcomeState,
            String extraHistogram) {
        var manager =
                new DefaultBrowserPromoManager(
                        mActivity,
                        mWindowAndroid,
                        mImpressionCounter,
                        mStateProvider,
                        /* source= */ null);

        String outcomeHistogram =
                currentState == DefaultBrowserState.NO_DEFAULT
                        ? "Android.DefaultBrowserPromo.Outcome.NoDefault"
                        : "Android.DefaultBrowserPromo.Outcome.OtherDefault";

        var histogramBuilder =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DefaultBrowserPromo.RoleManagerShown", currentState)
                        .expectIntRecord(outcomeHistogram, outcomeState);
        if (extraHistogram != null) {
            histogramBuilder.expectIntRecord(extraHistogram, outcomeState);
        }
        var histogram = histogramBuilder.build();

        doReturn(1).when(mWindowAndroid).showCancelableIntent(any(Intent.class), any(), any());
        when(mStateProvider.getCurrentDefaultBrowserState()).thenReturn(currentState, outcomeState);
        ArgumentCaptor<IntentCallback> onShowCallbackCaptor =
                ArgumentCaptor.forClass(IntentCallback.class);
        manager.promoByRoleManager();
        verify(mWindowAndroid)
                .showCancelableIntent(eq(mIntent), onShowCallbackCaptor.capture(), any());
        onShowCallbackCaptor.getValue().onIntentCompleted(1, null);
        histogram.assertExpected("BrowserState is not correctly recorded");
    }
}
