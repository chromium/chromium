// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container.dev;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.ui.side_panel_container.SidePanelContainerCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests {@link SidePanelDevFeature}'s integration with {@code ChromeActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@EnableFeatures({
    ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL,
    ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL_DEV_FEATURE
})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@NullMarked
public class SidePanelDevFeatureIntegrationTest {

    @Rule
    public final FreshCtaTransitTestRule mFreshCtaTransitTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Before
    public void setUp() {
        mFreshCtaTransitTestRule.startOnBlankPage();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL_DEV_FEATURE)
    public void devFeatureFlagDisabled_sidePanelDevFeatureDoesNotExist() {
        assertNull(getSidePanelDevFeature());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL + ":disable_animations/true")
    @DisabledTest(message = "b/512331243 - Deterministic failure on desktop emulators")
    public void toggle_toggleDevContent() {
        // Arrange.
        var sidePanelDevFeature = getSidePanelDevFeature();
        assertNotNull(sidePanelDevFeature);

        // Act: Toggle the dev feature.
        ThreadUtils.runOnUiThreadBlocking(sidePanelDevFeature::toggle);

        // Assert: The dev feature is shown.
        var sidePanelContainerCoordinator = getSidePanelContainerCoordinator();
        var sidePanelDevFeatureContent =
                ThreadUtils.runOnUiThreadBlocking(
                        sidePanelDevFeature::getDevFeatureContentForTesting);
        assertNotNull(sidePanelDevFeatureContent);
        var sidePanelContent = sidePanelDevFeatureContent.mSidePanelContent;
        assertNotNull(sidePanelContent);
        assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> sidePanelContainerCoordinator.isShowing(sidePanelContent)));

        // Act: Toggle the dev feature.
        ThreadUtils.runOnUiThreadBlocking(sidePanelDevFeature::toggle);

        // Assert: The dev feature is hidden.
        assertFalse(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> sidePanelContainerCoordinator.isShowing(sidePanelContent)));
    }

    private TabbedRootUiCoordinator getTabbedRootUiCoordinator() {
        return (TabbedRootUiCoordinator)
                mFreshCtaTransitTestRule.getActivity().getRootUiCoordinatorForTesting();
    }

    private SidePanelContainerCoordinator getSidePanelContainerCoordinator() {
        var sidePanelContainerCoordinator =
                getTabbedRootUiCoordinator().getSidePanelContainerCoordinatorForTesting();
        assertNotNull(sidePanelContainerCoordinator);
        return sidePanelContainerCoordinator;
    }

    private @Nullable SidePanelDevFeatureImpl getSidePanelDevFeature() {
        return (SidePanelDevFeatureImpl)
                getTabbedRootUiCoordinator().getSidePanelDevFeatureForTesting();
    }
}
