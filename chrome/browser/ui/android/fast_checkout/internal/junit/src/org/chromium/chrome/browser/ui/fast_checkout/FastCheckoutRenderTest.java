// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType.AUTOFILL_PROFILE_SCREEN;
import static org.chromium.chrome.browser.ui.fast_checkout.FastCheckoutProperties.ScreenType.CREDIT_CARD_SCREEN;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.chrome.browser.ui.suggestion.Icon;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * These tests render screenshots of the Fast Checkout {@link BottomSheet} and compare them to a
 * gold standard.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisabledTest(message = "https://crbug.com/1424888")
public class FastCheckoutRenderTest {
    private static final FastCheckoutAutofillProfile AUTOFILL_PROFILE =
            FastCheckoutTestUtils.createDetailedProfile(
                    /* guid= */ "111",
                    /* name= */ "John Moe",
                    /* streetAddress= */ "Park Avenue 234",
                    /* city= */ "New York",
                    /* postalCode= */ "12345",
                    /* email= */ "john.moe@gmail.com",
                    /* phoneNumber= */ "(345) 543-645");
    private static final FastCheckoutCreditCard LOCAL_CREDIT_CARD =
            FastCheckoutTestUtils.createDetailedLocalCreditCard(
                    /* guid= */ "123",
                    /* origin= */ "https://example.com",
                    /* name= */ "John Moe",
                    /* number= */ "75675675656",
                    /* obfuscatedNumber= */ "• • • • 5656",
                    /* month= */ "05",
                    /* year= */ AutofillTestHelper.nextYear(),
                    /* issuerIcon= */ Icon.CARD_VISA);
    private static final FastCheckoutCreditCard SERVER_CREDIT_CARD =
            FastCheckoutTestUtils.createDetailedCreditCard(
                    /* guid= */ "123",
                    /* origin= */ "https://example.com",
                    /* isLocal= */ false,
                    /* name= */ "John Moe",
                    /* number= */ "75675675656",
                    /* obfuscatedNumber= */ "• • • • 5656",
                    /* month= */ "05",
                    /* year= */ AutofillTestHelper.nextYear(),
                    /* issuerIcon= */ Icon.CARD_VISA);

    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false, false).name("Default"),
                    new ParameterSet().value(false, true).name("RTL"),
                    new ParameterSet().value(true, false).name("NightMode"));

    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    // Increase revision number with each UI change if it invalidates previous
                    // golden images.
                    .setRevision(2)
                    .build();

    @Mock private FastCheckoutComponent.Delegate mDelegateMock;

    private BottomSheetController mBottomSheetController;
    private FastCheckoutCoordinator mCoordinator;

    public FastCheckoutRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
        setRtlForTesting(useRtlLayout);
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(useRtlLayout ? "RTL" : "LTR");
    }

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.waitForActivityCompletelyLoaded();
        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator = new FastCheckoutCoordinator();
                    mCoordinator.initialize(
                            mActivityTestRule.getActivity(), mBottomSheetController, mDelegateMock);
                });
    }

    @After
    public void tearDown() {
        setRtlForTesting(false);
        try {
            finishActivity(mActivityTestRule.getActivity());
        } catch (Exception e) {
            // Activity was already closed (e.g. due to last test tearing down the suite).
        }
        runOnUiThreadBlocking(() -> tearDownNightModeAfterChromeActivityDestroyed());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsHomeScreenWithLocalCreditCard() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showOptions(List.of(AUTOFILL_PROFILE), List.of(LOCAL_CREDIT_CARD));
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "fast_checkout_home_screen_local_card");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsHomeScreenWithServerCreditCard() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showOptions(
                            List.of(AUTOFILL_PROFILE), List.of(SERVER_CREDIT_CARD));
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "fast_checkout_home_screen_server_card");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsAddressesScreen() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator
                            .getModelForTesting()
                            .set(FastCheckoutProperties.CURRENT_SCREEN, AUTOFILL_PROFILE_SCREEN);
                    mCoordinator.showOptions(
                            List.of(AUTOFILL_PROFILE), List.of(SERVER_CREDIT_CARD));
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "fast_checkout_addresses_screen");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowsCreditCardsScreen() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator
                            .getModelForTesting()
                            .set(FastCheckoutProperties.CURRENT_SCREEN, CREDIT_CARD_SCREEN);
                    mCoordinator.showOptions(
                            List.of(AUTOFILL_PROFILE), List.of(SERVER_CREDIT_CARD));
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView = mActivityTestRule.getActivity().findViewById(R.id.bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "fast_checkout_credit_cards_screen");
    }
}
