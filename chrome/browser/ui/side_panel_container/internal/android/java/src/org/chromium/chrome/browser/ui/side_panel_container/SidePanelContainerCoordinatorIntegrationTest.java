// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.ui.side_panel_container.test.SidePanelContainerCoordinatorIntegrationTestSupport;
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
@EnableFeatures({
    ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL,
    ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL_DEV_FEATURE + ":scope/tab"
})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@NullMarked
public class SidePanelContainerCoordinatorIntegrationTest {
    private static final String RESPONSIVE_WEB_PAGE_URL =
            "/chrome/browser/ui/side_panel_container/test/data/responsive_page.html";

    private WebPageStation mResponsivePageStation;

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
    }

    @Test
    @MediumTest
    public void showPanel_addsContentView() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();

        // Act.
        showPanel(mResponsivePageStation.getTab());
        FrameLayout containerView = waitForContainerViewOpen(coordinator);

        // Assert.
        assertEquals(1, containerView.getChildCount());
        assertNotNull(containerView.getChildAt(0));
    }

    @Test
    @MediumTest
    public void replacePanelContent_replacesContentView() {
        // Arrange: Show the side panel for the current active tab.
        var coordinator = getSidePanelContainerCoordinator();
        var tab1 = mResponsivePageStation.getTab();
        showPanel(tab1);
        FrameLayout containerView = waitForContainerViewOpen(coordinator);
        assertEquals(1, containerView.getChildCount());
        View contentView1 = containerView.getChildAt(0);

        // Arrange: Show the side panel for a new tab.
        var newTabPageStation = mResponsivePageStation.openNewTabFast();
        var tab2 = newTabPageStation.getTab();
        showPanel(tab2);
        waitForContainerViewOpen(coordinator);
        assertEquals(1, containerView.getChildCount());
        View contentView2 = containerView.getChildAt(0);
        assertNotEquals(contentView1, contentView2);

        // Act: Switch back to the first tab.
        mResponsivePageStation = newTabPageStation.selectTabFast(tab1, WebPageStation::newBuilder);
        waitForContainerViewOpen(coordinator);

        // Assert.
        assertEquals(1, containerView.getChildCount());
        assertEquals(contentView1, containerView.getChildAt(0));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void showPanel_renderContainer() throws Exception {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();

        // Act.
        showPanel(mResponsivePageStation.getTab());
        FrameLayout containerView = waitForContainerViewOpen(coordinator);

        // Assert.
        mRenderTestRule.render(containerView, "side_panel_container");
    }

    @Test
    @MediumTest
    public void closePanel_removesContentView() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var tab = mResponsivePageStation.getTab();
        showPanel(tab);
        FrameLayout containerView = waitForContainerViewOpen(coordinator);

        // Act.
        closePanel(tab);
        waitForContainerViewClose(coordinator);

        // Assert.
        assertEquals(0, containerView.getChildCount());
    }

    @Test
    @MediumTest
    public void openAndClosePanel_resizeWebContents() {
        // Arrange: Get WebContents.
        var tab = mResponsivePageStation.getTab();
        var webContents = tab.getWebContents();
        assertNotNull(webContents);
        int originalWebContentsWidth = ThreadUtils.runOnUiThreadBlocking(webContents::getWidth);

        // Act: Open the side panel.
        var coordinator = getSidePanelContainerCoordinator();
        showPanel(tab);
        waitForContainerViewOpen(coordinator);

        // Assert: The WebContents width should become smaller.
        //
        // Note: we choose not to assert the exact width of the WebContents as the
        // exact width is hard to obtain due to rounding errors during "dp<->px" conversion on
        // different bots (WebContents#getWidth() returns a value in dp).
        CriteriaHelper.pollUiThread(
                () -> webContents.getWidth() < originalWebContentsWidth,
                "WebContents width did not decrease.");
        int webContentsWidthAfterSidePanelOpen =
                ThreadUtils.runOnUiThreadBlocking(webContents::getWidth);

        // Act: Close the side panel.
        closePanel(tab);
        waitForContainerViewClose(coordinator);

        // Assert: The WebContents width should become larger.
        //
        // Similarly, we don't assert "webContents.getWidth() == originalWebContentsWidth" to avoid
        // rounding errors in "dp<->px" conversion.
        CriteriaHelper.pollUiThread(
                () -> webContents.getWidth() > webContentsWidthAfterSidePanelOpen,
                "WebContents width did not increase.");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void openAndClosePanel_tabThumbnailHasCorrectWidth() throws Exception {
        // Arrange: Get the tab showing the responsive page.
        var tab = mResponsivePageStation.getTab();

        // Arrange: Open the side panel.
        var coordinator = getSidePanelContainerCoordinator();
        showPanel(tab);
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
        closePanel(tab);
        waitForContainerViewClose(coordinator);

        // Act: Open the grid tab switcher again.
        regularTabSwitcherStation = mResponsivePageStation.openRegularTabSwitcher();
        tabCardFacility = regularTabSwitcherStation.expectTabCard(tab.getId(), tab.getTitle());
        tabCardView = tabCardFacility.cardViewElement.value();

        // Assert.
        mRenderTestRule.render(tabCardView, "tab_card_after_closing_side_panel");
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

    private static void showPanel(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        SidePanelContainerCoordinatorIntegrationTestSupport.showSidePanel(
                                tab, /* suppressAnimations= */ true));
    }

    private static void closePanel(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        SidePanelContainerCoordinatorIntegrationTestSupport.closeSidePanel(
                                tab, /* suppressAnimations= */ true));
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
