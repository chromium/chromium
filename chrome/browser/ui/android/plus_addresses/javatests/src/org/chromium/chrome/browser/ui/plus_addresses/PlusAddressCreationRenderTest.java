// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.ApplicationTestUtils.finishActivity;
import static org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed;
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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * These tests render screenshots of the plus address creation dialog and compare them to a gold
 * standard.
 */
@DoNotBatch(reason = "The tests can't be batched because they run for different set-ups.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PlusAddressCreationRenderTest {
    private static final String MANAGE_PLUS_ADDRESSES_DESCRIPTION = "For example@gmail.com.";
    private static final String PROPOSED_PLUS_ADDRESS = "example.foo@gmail.com";
    private static final PlusAddressCreationErrorStateInfo ERROR_STATE =
            new PlusAddressCreationErrorStateInfo(
                    PlusAddressCreationBottomSheetErrorType.RESERVE_TIMEOUT,
                    "Title",
                    "Description",
                    "Ok",
                    "Cancel");

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
                    .setRevision(1)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private PlusAddressCreationViewBridge mBridge;

    private BottomSheetController mBottomSheetController;
    private PlusAddressCreationCoordinator mCoordinator;

    public PlusAddressCreationRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
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
    }

    private void openBottomSheet(String description, boolean refreshSupported) {
        openBottomSheet(description, refreshSupported, "");
    }

    private void openBottomSheet(String description, boolean refreshSupported, String notice) {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new PlusAddressCreationCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mBottomSheetController,
                                    mLayoutStateProvider,
                                    mTabModel,
                                    mTabModelSelector,
                                    mBridge,
                                    new PlusAddressCreationNormalStateInfo(
                                            "Modal title",
                                            description,
                                            notice,
                                            "Plus address placeholder",
                                            "Accept",
                                            "Cancel",
                                            "Report an error <link>link</link>.",
                                            new GURL("https://help.google.com"),
                                            new GURL("https://error.google.com")),
                                    refreshSupported);
                    mCoordinator.requestShowContent();
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
    public void testShowBottomSheet() throws IOException {
        openBottomSheet(MANAGE_PLUS_ADDRESSES_DESCRIPTION, /* refreshSupported= */ false);
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView =
                mActivityTestRule.getActivity().findViewById(R.id.plus_address_dialog);
        mRenderTestRule.render(bottomSheetView, "show_bottom_sheet_with_redesign");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowBottomSheet_RefreshSupported() throws IOException {
        openBottomSheet(MANAGE_PLUS_ADDRESSES_DESCRIPTION, /* refreshSupported= */ true);
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView =
                mActivityTestRule.getActivity().findViewById(R.id.plus_address_dialog);
        mRenderTestRule.render(bottomSheetView, "show_bottom_sheet_with_redesign_and_refresh");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowProposedPlusAddress() throws IOException {
        openBottomSheet(MANAGE_PLUS_ADDRESSES_DESCRIPTION, /* refreshSupported= */ false);
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.updateProposedPlusAddress(PROPOSED_PLUS_ADDRESS);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView =
                mActivityTestRule.getActivity().findViewById(R.id.plus_address_dialog);
        mRenderTestRule.render(bottomSheetView, "show_bottom_sheet_with_error_and_redesign");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowProposedPlusAddress_UiRedesignEnabled_RefreshSupported()
            throws IOException {
        openBottomSheet(MANAGE_PLUS_ADDRESSES_DESCRIPTION, /* refreshSupported= */ true);
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.updateProposedPlusAddress(PROPOSED_PLUS_ADDRESS);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView =
                mActivityTestRule.getActivity().findViewById(R.id.plus_address_dialog);
        mRenderTestRule.render(
                bottomSheetView, "show_bottom_sheet_with_error_ui_redesign_and_refresh");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testLegacyErrorScreen() throws IOException {
        openBottomSheet(MANAGE_PLUS_ADDRESSES_DESCRIPTION, /* refreshSupported= */ false);
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showError(/* errorStateInfo= */ null);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView =
                mActivityTestRule.getActivity().findViewById(R.id.plus_address_dialog);
        mRenderTestRule.render(
                bottomSheetView, "plus_address_bottom_sheet_error_shown_with_redesign");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testReserveErrorMessageContent() throws IOException {
        openBottomSheet(MANAGE_PLUS_ADDRESSES_DESCRIPTION, /* refreshSupported= */ false);
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator.showError(ERROR_STATE);
                });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        View bottomSheetView =
                mActivityTestRule.getActivity().findViewById(R.id.plus_address_dialog);
        mRenderTestRule.render(bottomSheetView, "plus_address_bottom_sheet_reserve_error_shown");
    }
}
