// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel.dev;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.ui.side_panel.SidePanelContainerCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiContainer;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests {@link SidePanelDevFeature}'s integration with {@code ChromeActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL + ":disable_animations/true")
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@NullMarked
public class SidePanelDevFeatureIntegrationTest {

    @Rule
    public final FreshCtaTransitTestRule mFreshCtaTransitTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mInitialWebPageStation;

    @Before
    public void setUp() {
        mInitialWebPageStation = mFreshCtaTransitTestRule.startOnBlankPage();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL_DEV_FEATURE)
    public void devFeatureFlagDisabled_sidePanelDevFeatureDoesNotExist() {
        assertNull(getSidePanelDevFeature());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL_DEV_FEATURE)
    public void testWindowScopedDevFeature() {
        // Arrange: Open 2 tabs.
        var tab1 = mInitialWebPageStation.getTab();
        var newTabPageStation = mInitialWebPageStation.openNewTabFast();
        var tab2 = newTabPageStation.getTab();

        // Act: Toggle the dev feature.
        var sidePanelDevFeature = getSidePanelDevFeature();
        assertNotNull(sidePanelDevFeature);
        ThreadUtils.runOnUiThreadBlocking(sidePanelDevFeature::toggle);

        // Assert: Side panel is open.
        waitForSidePanelOpen();

        // Act: Switch tabs.
        mInitialWebPageStation = newTabPageStation.selectTabFast(tab1, WebPageStation::newBuilder);

        // Assert: Side panel is still open.
        waitForSidePanelOpen();

        // Act: Toggle again.
        ThreadUtils.runOnUiThreadBlocking(sidePanelDevFeature::toggle);

        // Assert: Side panel is closed.
        waitForSidePanelClose();

        // Act: Switch tabs again.
        mInitialWebPageStation.selectTabFast(tab2, RegularNewTabPageStation::newBuilder);

        // Assert: Side panel is still closed.
        waitForSidePanelClose();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL_DEV_FEATURE + ":scope/tab")
    public void testTabScopedDevFeature() {
        // Arrange: Open 2 tabs.
        var tab1 = mInitialWebPageStation.getTab();
        var newTabPageStation = mInitialWebPageStation.openNewTabFast();
        var tab2 = newTabPageStation.getTab();

        // Act: Toggle the dev feature.
        var sidePanelDevFeature = getSidePanelDevFeature();
        assertNotNull(sidePanelDevFeature);
        ThreadUtils.runOnUiThreadBlocking(sidePanelDevFeature::toggle);

        // Assert: Side panel is open.
        waitForSidePanelOpen();

        // Act: Switch tabs.
        mInitialWebPageStation = newTabPageStation.selectTabFast(tab1, WebPageStation::newBuilder);

        // Assert: Side panel is closed.
        waitForSidePanelClose();

        // Act: Switch tabs again.
        newTabPageStation =
                mInitialWebPageStation.selectTabFast(tab2, RegularNewTabPageStation::newBuilder);

        // Assert: Side panel is open.
        waitForSidePanelOpen();

        // Act: Toggle again.
        ThreadUtils.runOnUiThreadBlocking(sidePanelDevFeature::toggle);

        // Assert: Side panel is closed.
        waitForSidePanelClose();
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

    private @Nullable SidePanelDevFeature getSidePanelDevFeature() {
        return getTabbedRootUiCoordinator().getSidePanelDevFeatureForTesting();
    }

    private void waitForSidePanelOpen() {
        var sideUiContainer = (SideUiContainer) getSidePanelContainerCoordinator();
        View containerView = ThreadUtils.runOnUiThreadBlocking(sideUiContainer::getView);

        CriteriaHelper.pollUiThread(
                () -> containerView.getWidth() > 0,
                "The container View should have been attached and laid out.");
    }

    private void waitForSidePanelClose() {
        var sideUiContainer = (SideUiContainer) getSidePanelContainerCoordinator();
        View containerView = ThreadUtils.runOnUiThreadBlocking(sideUiContainer::getView);

        CriteriaHelper.pollUiThread(
                () -> containerView.getParent() == null,
                "The container View should have been detached.");
    }
}
