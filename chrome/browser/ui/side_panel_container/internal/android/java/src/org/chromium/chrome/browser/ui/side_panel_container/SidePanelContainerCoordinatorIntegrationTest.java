// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.annotation.SuppressLint;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.RenderTestRule;

/** Tests {@link SidePanelContainerCoordinatorImpl}'s integration with {@code ChromeActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@NullMarked
public class SidePanelContainerCoordinatorIntegrationTest {
    private static final String RESPONSIVE_WEB_PAGE_URL =
            "/chrome/browser/ui/side_panel_container/test/data/responsive_page.html";
    private static final @ColorInt int SIDE_PANEL_CONTENT_BACKGROUND_COLOR =
            Color.rgb(204, 85, 0); // Dark Orange

    private WebPageStation mResponsivePageStation;
    private Callback<@Nullable Void> mOnAnimationFinishedCallbackMock;

    @Rule
    public final FreshCtaTransitTestRule mFreshCtaTransitTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_TOP_CHROME_SIDE_PANEL)
                    .build();

    @Before
    public void setUp() {
        String responsivePageUrl =
                mFreshCtaTransitTestRule.getTestServer().getURL(RESPONSIVE_WEB_PAGE_URL);
        mResponsivePageStation = mFreshCtaTransitTestRule.startOnUrl(responsivePageUrl);
        ChromeTabUtils.waitForTabPageLoaded(mResponsivePageStation.getTab(), responsivePageUrl);

        mOnAnimationFinishedCallbackMock = result -> {};
    }

    @Test
    @MediumTest
    public void populateContent_containerHasNoContent_addsContentView() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content");

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent,
                                mOnAnimationFinishedCallbackMock,
                                /* startingBounds= */ null,
                                true));
        FrameLayout containerView = waitForContainerViewOpen(coordinator);

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
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent1,
                                mOnAnimationFinishedCallbackMock,
                                /* startingBounds= */ null,
                                true));
        waitForContainerViewOpen(coordinator);

        // Act.
        var sidePanelContent2 = createSidePanelContent("Side Panel Content 2");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent2,
                                mOnAnimationFinishedCallbackMock,
                                /* startingBounds= */ null,
                                true));
        FrameLayout containerView = waitForContainerViewOpen(coordinator);

        // Assert.
        assertEquals(1, containerView.getChildCount());
        assertEquals(sidePanelContent2.mView, containerView.getChildAt(0));
    }

    @Test
    @MediumTest
    public void populateContent_withStartingBounds_addsContentView() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content");
        Rect startingBounds = new Rect(10, 20, 110, 220);

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent,
                                mOnAnimationFinishedCallbackMock,
                                startingBounds,
                                true));
        FrameLayout containerView = waitForContainerViewOpen(coordinator);

        // Assert.
        assertEquals(1, containerView.getChildCount());
        assertEquals(sidePanelContent.mView, containerView.getChildAt(0));
    }

    @Test
    @MediumTest
    public void populateContent_containerViewHasValidWidth() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content");

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent,
                                mOnAnimationFinishedCallbackMock,
                                /* startingBounds= */ null,
                                true));

        // Assert.
        //
        // Note: we choose not to assert the exact width of the side panel container view as the
        // exact width is hard to obtain due to rounding errors during "dp<->px" conversion on
        // different bots.
        waitForContainerViewOpen(coordinator);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void populateContent_renderContainer() throws Exception {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content");

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent,
                                mOnAnimationFinishedCallbackMock,
                                /* startingBounds= */ null,
                                /* suppressAnimations= */ true));
        FrameLayout containerView = waitForContainerViewOpen(coordinator);

        // Assert.
        mRenderTestRule.render(containerView, "side_panel_container");
    }

    @Test
    @MediumTest
    public void removeContent_removesContentAndCloseView() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content To Remove");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent,
                                mOnAnimationFinishedCallbackMock,
                                /* startingBounds= */ null,
                                true));
        FrameLayout containerView = waitForContainerViewOpen(coordinator);

        // Act.
        ThreadUtils.runOnUiThreadBlocking(
                () -> coordinator.removeContentAndClose(mOnAnimationFinishedCallbackMock, true));
        waitForContainerViewClose(coordinator);

        // Assert.
        assertEquals(0, containerView.getChildCount());
    }

    @Test
    @MediumTest
    public void populateAndRemoveContent_resizeWebContents() {
        // Arrange: Get WebContents.
        var webContents = mResponsivePageStation.getTab().getWebContents();
        assertNotNull(webContents);
        int originalWebContentsWidth = webContents.getWidth();

        // Act: Open the side panel.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent,
                                mOnAnimationFinishedCallbackMock,
                                /* startingBounds= */ null,
                                /* suppressAnimations= */ true));
        waitForContainerViewOpen(coordinator);

        // Assert: The WebContents width should become smaller.
        //
        // Note: we choose not to assert the exact width of the WebContents as the
        // exact width is hard to obtain due to rounding errors during "dp<->px" conversion on
        // different bots (WebContents#getWidth() returns a value in dp).
        int webContentsWidthAfterSidePanelOpen = webContents.getWidth();
        assertTrue(webContentsWidthAfterSidePanelOpen < originalWebContentsWidth);

        // Act: Close the side panel.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.removeContentAndClose(
                                mOnAnimationFinishedCallbackMock, /* suppressAnimations= */ true));
        waitForContainerViewClose(coordinator);

        // Assert: The WebContents width should become larger.
        //
        // Similarly, we don't assert "webContents.getWidth() == originalWebContentsWidth" to avoid
        // rounding errors in "dp<->px" conversion.
        int webContentsWidthAfterSidePanelClose = webContents.getWidth();
        assertTrue(webContentsWidthAfterSidePanelClose > webContentsWidthAfterSidePanelOpen);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void populateAndRemoveContent_tabThumbnailHasCorrectWidth() throws Exception {
        // Arrange: Get the tab showing the responsive page.
        var tab = mResponsivePageStation.getTab();

        // Arrange: Open the side panel.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent,
                                mOnAnimationFinishedCallbackMock,
                                /* startingBounds= */ null,
                                /* suppressAnimations= */ true));
        waitForContainerViewOpen(coordinator);

        // Act: Open the grid tab switcher.
        var regularTabSwitcherStation = mResponsivePageStation.openRegularTabSwitcher();
        var tabCardFacility = regularTabSwitcherStation.expectTabCard(tab.getId(), tab.getTitle());
        View tabCardView = tabCardFacility.cardViewElement.value();

        // Assert.
        mRenderTestRule.render(tabCardView, "tab_card_after_opening_side_panel");

        // Arrange: Close the tab switcher by selecting the tab.
        // Note that the side panel should still be shown.
        mResponsivePageStation = tabCardFacility.clickCard(WebPageStation.newBuilder());
        waitForContainerViewOpen(coordinator);

        // Arrange: Close the side panel.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.removeContentAndClose(
                                mOnAnimationFinishedCallbackMock, /* suppressAnimations= */ true));
        waitForContainerViewClose(coordinator);

        // Act: Open the grid tab switcher again.
        regularTabSwitcherStation = mResponsivePageStation.openRegularTabSwitcher();
        tabCardFacility = regularTabSwitcherStation.expectTabCard(tab.getId(), tab.getTitle());
        tabCardView = tabCardFacility.cardViewElement.value();

        // Assert.
        mRenderTestRule.render(tabCardView, "tab_card_after_closing_side_panel");
    }

    @Test
    @MediumTest
    public void isShowingContent_beforePopulatingContent_returnsFalse() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content");

        // Assert.
        assertFalse(
                ThreadUtils.runOnUiThreadBlocking(() -> coordinator.isShowing(sidePanelContent)));
    }

    @Test
    @MediumTest
    public void isShowing_containerHasContent_returnsTrueForSameContent() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent,
                                mOnAnimationFinishedCallbackMock,
                                /* startingBounds= */ null,
                                true));
        waitForContainerViewOpen(coordinator);

        // Assert.
        assertTrue(
                ThreadUtils.runOnUiThreadBlocking(() -> coordinator.isShowing(sidePanelContent)));
    }

    @Test
    @MediumTest
    public void isShowing_containerHasContent_returnsFalseForDifferentContent() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent1 = createSidePanelContent("Side Panel Content 1");
        var sidePanelContent2 = createSidePanelContent("Side Panel Content 2");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent1,
                                mOnAnimationFinishedCallbackMock,
                                /* startingBounds= */ null,
                                true));
        waitForContainerViewOpen(coordinator);

        // Assert.
        assertFalse(
                ThreadUtils.runOnUiThreadBlocking(() -> coordinator.isShowing(sidePanelContent2)));
    }

    @Test
    @MediumTest
    public void isShowing_afterRemovingContentAndClose_returnsFalse() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var sidePanelContent = createSidePanelContent("Side Panel Content To Remove");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        coordinator.populateContent(
                                sidePanelContent,
                                mOnAnimationFinishedCallbackMock,
                                /* startingBounds= */ null,
                                true));
        waitForContainerViewOpen(coordinator);
        ThreadUtils.runOnUiThreadBlocking(
                () -> coordinator.removeContentAndClose(mOnAnimationFinishedCallbackMock, true));
        waitForContainerViewClose(coordinator);

        // Assert.
        assertFalse(
                ThreadUtils.runOnUiThreadBlocking(() -> coordinator.isShowing(sidePanelContent)));
    }

    @SuppressLint("SetTextI18n")
    private SidePanelContent createSidePanelContent(String contentText) {
        TextView contentView = new TextView(mFreshCtaTransitTestRule.getActivity());
        contentView.setText(contentText);
        contentView.setTextAppearance(
                org.chromium.ui.R.style.TextAppearance_Headline_Primary_Baseline);
        contentView.setBackgroundColor(SIDE_PANEL_CONTENT_BACKGROUND_COLOR);
        contentView.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        contentView.setGravity(Gravity.CENTER);
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
    private static FrameLayout waitForContainerViewOpen(
            SidePanelContainerCoordinatorImpl coordinator) {
        View containerView = ThreadUtils.runOnUiThreadBlocking(coordinator::getView);
        assertTrue(containerView instanceof FrameLayout);

        CriteriaHelper.pollUiThread(
                () -> containerView.getWidth() > 0,
                "The container View should have been attached and laid out.");
        return (FrameLayout) containerView;
    }

    /** Waits for the View of {@link SidePanelContainerCoordinator} to be detached. */
    private static void waitForContainerViewClose(SidePanelContainerCoordinatorImpl coordinator) {
        View containerView = ThreadUtils.runOnUiThreadBlocking(coordinator::getView);

        CriteriaHelper.pollUiThread(
                () -> containerView.getParent() == null,
                "The container View should have been detached.");
    }
}
