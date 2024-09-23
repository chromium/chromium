// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.plus_addresses;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.base.LocalizationUtils.setRtlForTesting;

import android.view.View;

import androidx.test.filters.MediumTest;

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
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * These tests render screenshots of the all plus addresses bottom sheet and compare them to a gold
 * standard.
 */
@DoNotBatch(reason = "The tests can't be batched because they run for different set-ups.")
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AllPlusAddressesBottomSheetRenderTest {
    private static final String BOTTOMSHEET_TITLE = "Bottom sheet title";
    private static final String BOTTOMSHEET_WARNING = "Bottom sheet warning";
    private static final String BOTTOMSHEET_QUERY_HINT = "Query hint";
    private static final PlusProfile PROFILE_1 =
            new PlusProfile("foo@gmail.com", "google.com", "https://google.com");
    private static final PlusProfile PROFILE_2 =
            new PlusProfile("bar@gmail.com", "amazon.com", "https://amazon.com");
    private static final PlusProfile PROFILE_3 =
            new PlusProfile("example@gmail.com", "facebook.com", "https://facebook.com");
    private static final PlusProfile PROFILE_4 =
            new PlusProfile("lake@gmail.com", "microsoft.com", "http://microsoft.com");

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

    @Mock private Profile mProfile;
    @Mock private AllPlusAddressesBottomSheetCoordinator.Delegate mDelegate;

    public AllPlusAddressesBottomSheetRenderTest(boolean nightModeEnabled, boolean useRtlLayout) {
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
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowBottomSheet() throws IOException {
        runOnUiThreadBlocking(
                () -> {
                    AllPlusAddressesBottomSheetUIInfo uiInfo =
                            new AllPlusAddressesBottomSheetUIInfo();
                    uiInfo.setTitle(BOTTOMSHEET_TITLE);
                    uiInfo.setWarning(BOTTOMSHEET_WARNING);
                    uiInfo.setQueryHint(BOTTOMSHEET_QUERY_HINT);
                    uiInfo.setPlusProfiles(List.of(PROFILE_1, PROFILE_2, PROFILE_3, PROFILE_4));

                    ChromeTabbedActivity activity = mActivityTestRule.getActivity();
                    AllPlusAddressesBottomSheetCoordinator coordinator =
                            new AllPlusAddressesBottomSheetCoordinator(
                                    activity,
                                    activity.getRootUiCoordinatorForTesting()
                                            .getBottomSheetController(),
                                    mDelegate,
                                    FaviconHelper.create(activity, mProfile));

                    coordinator.showPlusProfiles(uiInfo);
                });
        BottomSheetTestSupport.waitForOpen(
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController());

        View bottomSheetView =
                mActivityTestRule.getActivity().findViewById(R.id.all_plus_addresses_bottom_sheet);
        mRenderTestRule.render(bottomSheetView, "all_plus_addresses_bottom_sheet");
    }
}
