// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.annotation.SuppressLint;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

/** Tests {@link SidePanelContainerCoordinatorImpl}'s integration with {@code ChromeActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@NullMarked
public class SidePanelContainerCoordinatorIntegrationTest {
    private static final @ColorInt int SIDE_PANEL_CONTENT_BACKGROUND_COLOR = Color.GREEN;

    @Rule
    public final FreshCtaTransitTestRule mFreshCtaTransitTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Before
    public void setUp() {
        mFreshCtaTransitTestRule.startOnBlankPage();
    }

    @Test
    @MediumTest
    public void populateContent_containerHasNoContent_addsContentView() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content");

        // Act.
        ThreadUtils.runOnUiThreadBlocking(() -> coordinator.populateContent(sidePanelContent));
        FrameLayout containerView = waitForContainerViewWithValidWidth(coordinator);

        // Assert.
        assertEquals(1, containerView.getChildCount());
        assertEquals(sidePanelContent.mView, containerView.getChildAt(0));
    }

    @Test
    @MediumTest
    public void populateContent_containerHasExistingContent_replacesContentView() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent1 = createSidePanelContent("Side Panel Content 1");
        ThreadUtils.runOnUiThreadBlocking(() -> coordinator.populateContent(sidePanelContent1));
        waitForContainerViewWithValidWidth(coordinator);

        // Act.
        var sidePanelContent2 = createSidePanelContent("Side Panel Content 2");
        ThreadUtils.runOnUiThreadBlocking(() -> coordinator.populateContent(sidePanelContent2));
        FrameLayout containerView = waitForContainerViewWithValidWidth(coordinator);

        // Assert.
        assertEquals(1, containerView.getChildCount());
        assertEquals(sidePanelContent2.mView, containerView.getChildAt(0));
    }

    @Test
    @MediumTest
    public void populateContent_containerViewHasCorrectWidth() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content");

        // Act.
        ThreadUtils.runOnUiThreadBlocking(() -> coordinator.populateContent(sidePanelContent));
        FrameLayout containerView = waitForContainerViewWithValidWidth(coordinator);

        // Assert.
        @Px
        int expectedWidth =
                ViewUtils.dpToPx(
                        mFreshCtaTransitTestRule.getActivity(),
                        SidePanelContainerCoordinatorImpl.SIDE_PANEL_MIN_WIDTH_DP);
        assertEquals(expectedWidth, containerView.getWidth());
    }

    @Test
    @MediumTest
    public void removeContent_removesContentView() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content To Remove");
        ThreadUtils.runOnUiThreadBlocking(() -> coordinator.populateContent(sidePanelContent));
        FrameLayout containerView = waitForContainerViewWithValidWidth(coordinator);

        // Act.
        ThreadUtils.runOnUiThreadBlocking(coordinator::removeContent);

        // Assert.
        assertEquals(0, containerView.getChildCount());
    }

    @SuppressLint("SetTextI18n")
    private SidePanelContent createSidePanelContent(String contentText) {
        TextView contentView = new TextView(mFreshCtaTransitTestRule.getActivity());
        contentView.setText(contentText);
        contentView.setBackgroundColor(SIDE_PANEL_CONTENT_BACKGROUND_COLOR);
        contentView.setLayoutParams(
                new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        return new SidePanelContent(contentView);
    }

    private SidePanelContainerCoordinatorImpl getSidePanelContainerCoordinator() {
        var sidePanelContainerCoordinator =
                ((TabbedRootUiCoordinator)
                                mFreshCtaTransitTestRule
                                        .getActivity()
                                        .getRootUiCoordinatorForTesting())
                        .getSidePanelContainerCoordinatorForTesting();
        assertNotNull(sidePanelContainerCoordinator);
        return (SidePanelContainerCoordinatorImpl) sidePanelContainerCoordinator;
    }

    /**
     * Waits for the View of {@link SidePanelContainerCoordinator} to have non-zero width.
     *
     * @return The View as returned by {@link SidePanelContainerCoordinatorImpl#getView()}.
     */
    private static FrameLayout waitForContainerViewWithValidWidth(
            SidePanelContainerCoordinatorImpl coordinator) {
        View containerView = ThreadUtils.runOnUiThreadBlocking(coordinator::getView);
        assertTrue(containerView instanceof FrameLayout);

        CriteriaHelper.pollUiThread(
                () -> containerView.getWidth() > 0,
                "The container View should have been attached and laid out.");
        return (FrameLayout) containerView;
    }
}
