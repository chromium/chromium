// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import static java.util.Collections.emptyList;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;

import androidx.annotation.NonNull;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.payments.PaymentApp.PaymentEntityLogo;
import org.chromium.components.payments.R;
import org.chromium.components.payments.SPCTransactionMode;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class SecurePaymentConfirmationRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet()
                            .value(
                                    /* darkModeEnabled */ false, /* useRtlLayout */
                                    false, /* informOnly */
                                    false, /* variantPrefix */
                                    "")
                            .name("LightMode"),
                    new ParameterSet()
                            .value(
                                    /* darkModeEnabled */ false, /* useRtlLayout */
                                    true, /* informOnly */
                                    false, /* variantPrefix */
                                    "RTL")
                            .name("RTL"),
                    new ParameterSet()
                            .value(
                                    /* darkModeEnabled */ true, /* useRtlLayout */
                                    false, /* informOnly */
                                    false, /* variantPrefix */
                                    "")
                            .name("DarkMode"),
                    new ParameterSet()
                            .value(
                                    /* darkModeEnabled */ false, /* useRtlLayout */
                                    false, /* informOnly */
                                    true, /* variantPrefix */
                                    "InformOnly")
                            .name("InformOnlyLightMode"),
                    new ParameterSet()
                            .value(
                                    /* darkModeEnabled */ false, /* useRtlLayout */
                                    true, /* informOnly */
                                    true, /* variantPrefix */
                                    "InformOnlyRTL")
                            .name("InformOnlyRTLLightMode"),
                    new ParameterSet()
                            .value(
                                    /* darkModeEnabled */ true, /* useRtlLayout */
                                    false, /* informOnly */
                                    true, /* variantPrefix */
                                    "InformOnly")
                            .name("InformOnlyDarkMode"));

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(0)
                    .setBugComponent(Component.BLINK_PAYMENTS)
                    .build();

    static class TestPaymentEntityLogo implements PaymentEntityLogo {
        private final Bitmap mIcon;
        private final String mLabel;

        TestPaymentEntityLogo(Bitmap icon, String label) {
            mIcon = icon;
            mLabel = label;
        }

        @Override
        public String getLabel() {
            return mLabel;
        }

        @Override
        public Bitmap getIcon() {
            return mIcon;
        }
    }

    private BottomSheetController mBottomSheetController;
    private final boolean mInformOnly;

    public SecurePaymentConfirmationRenderTest(
            boolean darkModeEnabled,
            boolean useRtlLayout,
            boolean informOnly,
            String variantPrefix) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(darkModeEnabled);
        setRtlForTesting(useRtlLayout);
        mInformOnly = informOnly;

        mRenderTestRule.setNightModeEnabled(darkModeEnabled);
        mRenderTestRule.setVariantPrefix(variantPrefix);
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();
        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
    }

    @After
    public void tearDown() {
        try {
            finishActivity(mActivityTestRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testShow() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    createController(
                                    /* showOptOut= */ false,
                                    /* primaryLabelsOnly= */ false,
                                    /* numberOfPaymentEntitiesLogos= */ 2)
                            .show();
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet), "default");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testShowOnePaymentEntityLogos() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    createController(
                                    /* showOptOut= */ false,
                                    /* primaryLabelsOnly= */ false,
                                    /* numberOfPaymentEntitiesLogos= */ 1)
                            .show();
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet),
                "one_payment_entity_logo");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testShowZeroPaymentEntityLogos() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    createController(
                                    /* showOptOut= */ false,
                                    /* primaryLabelsOnly= */ false,
                                    /* numberOfPaymentEntitiesLogos= */ 0)
                            .show();
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet),
                "zero_payment_entities_logos");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testShowOptOut() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    createController(
                                    /* showOptOut= */ true,
                                    /* primaryLabelsOnly= */ false,
                                    /* numberOfPaymentEntitiesLogos= */ 2)
                            .show();
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet), "opt_out");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testShowInformOnlyPrimaryLabelsOnly() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    createController(
                                    /* showOptOut= */ false,
                                    /* primaryLabelsOnly= */ true,
                                    /* numberOfPaymentEntitiesLogos= */ 2)
                            .show();
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet),
                "primary_labels_only");
    }

    @NonNull
    private SecurePaymentConfirmationController createController(
            boolean showOptOut, boolean primaryLabelsOnly, int numberOfPaymentEntitiesLogos) {
        PaymentItem paymentItem = new PaymentItem();
        paymentItem.amount = new PaymentCurrencyAmount();
        paymentItem.amount.currency = "CAD";
        paymentItem.amount.value = "1.50";

        int[] greenBitmapArray = new int[100];
        Arrays.fill(greenBitmapArray, Color.GREEN);
        int[] blueBitmapArray = new int[100];
        Arrays.fill(blueBitmapArray, Color.BLUE);

        assertThat(numberOfPaymentEntitiesLogos).isAtLeast(0);
        assertThat(numberOfPaymentEntitiesLogos).isLessThan(3);

        List<PaymentEntityLogo> paymentEntityLogos =
                switch (numberOfPaymentEntitiesLogos) {
                    case 1 -> List.of(
                            new TestPaymentEntityLogo(
                                    Bitmap.createBitmap(
                                            greenBitmapArray,
                                            /* width= */ 20,
                                            /* height= */ 5,
                                            Config.ARGB_8888),
                                    "logo label"));
                    case 2 -> List.of(
                            new TestPaymentEntityLogo(
                                    Bitmap.createBitmap(
                                            greenBitmapArray,
                                            /* width= */ 20,
                                            /* height= */ 5,
                                            Config.ARGB_8888),
                                    "first logo label"),
                            new TestPaymentEntityLogo(
                                    Bitmap.createBitmap(
                                            blueBitmapArray,
                                            /* width= */ 20,
                                            /* height= */ 5,
                                            Config.ARGB_8888),
                                    "second logo label"));
                    default -> emptyList();
                };

        return new SecurePaymentConfirmationController(
                Objects.requireNonNull(mActivityTestRule.getActivity().getWindowAndroid()),
                paymentEntityLogos,
                /* payeeName= */ "The Store",
                /* payeeOrigin= */ primaryLabelsOnly
                        ? null
                        : Origin.create(new GURL("https://test.store")),
                /* paymentInstrumentLabelPrimary= */ "My Credit Card",
                /* paymentInstrumentLabelSecondary= */ primaryLabelsOnly ? null : "•••• 1234",
                paymentItem,
                new BitmapDrawable(
                        ContextUtils.getApplicationContext().getResources(),
                        Bitmap.createBitmap(
                                new int[] {Color.RED},
                                /* width= */ 1,
                                /* height= */ 1,
                                Bitmap.Config.ARGB_8888)),
                /* relyingPartyId= */ "test.store",
                showOptOut,
                mInformOnly,
                /* responseCallback= */ result -> {},
                SPCTransactionMode.NONE);
    }
}
