// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Px;
import androidx.core.view.ViewCompat;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;
import androidx.window.layout.WindowMetricsCalculator;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.ui.side_panel_container.test.SidePanelContainerCoordinatorIntegrationTestSupport;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.util.ColorUtils;

import java.util.concurrent.atomic.AtomicReference;

/** Tests {@link SidePanelContainerCoordinatorImpl}'s integration with {@code ChromeActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Need to reset theme for consistent render test results")
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

    @AfterClass
    public static void tearDownAfterClass() {
        ThreadUtils.runOnUiThreadBlocking(
                ChromeNightModeTestUtils::tearDownNightModeAfterChromeActivityDestroyed);
    }

    @Test
    @MediumTest
    public void showPanel_addsContentView() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();

        // Act.
        showPanel(mResponsivePageStation.getTab());
        ViewGroup containerView = waitForContainerViewOpen(coordinator);

        // Assert.
        ViewGroup contentContainer = containerView.findViewById(R.id.side_panel_content_container);
        assertNotNull(contentContainer);
        assertEquals(1, contentContainer.getChildCount());
        assertNotNull(contentContainer.getChildAt(0));

        TextView titleView = containerView.findViewById(R.id.side_panel_title);
        assertNotNull(titleView);
        assertEquals("Developer Panel", titleView.getText().toString());
    }

    @Test
    @MediumTest
    public void replacePanelContent_replacesContentView() {
        // Arrange: Show the side panel for the current active tab.
        var coordinator = getSidePanelContainerCoordinator();
        var tab1 = mResponsivePageStation.getTab();
        showPanel(tab1);
        ViewGroup containerView = waitForContainerViewOpen(coordinator);
        ViewGroup contentContainer = containerView.findViewById(R.id.side_panel_content_container);
        assertNotNull(contentContainer);
        assertEquals(1, contentContainer.getChildCount());
        View contentView1 = contentContainer.getChildAt(0);

        // Arrange: Show the side panel for a new tab.
        var newTabPageStation = mResponsivePageStation.openNewTabFast();
        var tab2 = newTabPageStation.getTab();
        showPanel(tab2);
        waitForContainerViewOpen(coordinator);
        assertEquals(1, contentContainer.getChildCount());
        View contentView2 = contentContainer.getChildAt(0);
        assertNotEquals(contentView1, contentView2);

        // Act: Switch back to the first tab.
        mResponsivePageStation = newTabPageStation.selectTabFast(tab1, WebPageStation::newBuilder);
        waitForContainerViewOpen(coordinator);

        // Assert.
        assertEquals(1, contentContainer.getChildCount());
        assertEquals(contentView1, contentContainer.getChildAt(0));
    }

    @Test
    @MediumTest
    public void showPanel_setsAccessibilityPaneTitle() {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();
        var tab1 = mResponsivePageStation.getTab();

        // Act.
        showPanel(tab1);
        ViewGroup containerView = waitForContainerViewOpen(coordinator);

        // Assert.
        // We verify that the default dev feature sets the title correctly.
        CriteriaHelper.pollUiThread(
                () -> "Developer Panel".equals(ViewCompat.getAccessibilityPaneTitle(containerView)),
                "Accessibility pane title was not set correctly on open.");
    }

    @Test
    @MediumTest
    public void replacePanelContent_setsAccessibilityPaneTitle() {
        // Arrange: Show the side panel for the current active tab.
        var coordinator = getSidePanelContainerCoordinator();
        var tab1 = mResponsivePageStation.getTab();
        showPanel(tab1);
        ViewGroup containerView = waitForContainerViewOpen(coordinator);

        // Arrange: Show the side panel for a new tab.
        var newTabPageStation = mResponsivePageStation.openNewTabFast();
        var tab2 = newTabPageStation.getTab();
        showPanel(tab2);
        waitForContainerViewOpen(coordinator);

        // Force set a title on the view so we can test that replacing the panel natively updates it
        ThreadUtils.runOnUiThreadBlocking(
                () -> ViewCompat.setAccessibilityPaneTitle(containerView, "Test Custom Title"));

        // Act: Switch back to the first tab, natively replacing the panel content.
        mResponsivePageStation = newTabPageStation.selectTabFast(tab1, WebPageStation::newBuilder);
        waitForContainerViewOpen(coordinator);

        // Assert: The accessibility title matches the native testing feature's title.
        CriteriaHelper.pollUiThread(
                () -> "Developer Panel".equals(ViewCompat.getAccessibilityPaneTitle(containerView)),
                "Accessibility pane title was not updated correctly on replace.");
    }

    @Test
    @MediumTest
    public void closePanel_clearsAccessibilityPaneTitle() {
        // Arrange: Open the panel
        var coordinator = getSidePanelContainerCoordinator();
        var tab1 = mResponsivePageStation.getTab();
        showPanel(tab1);
        ViewGroup containerView = waitForContainerViewOpen(coordinator);

        // Force set a title on the view so we can test that close clears it
        ThreadUtils.runOnUiThreadBlocking(
                () -> ViewCompat.setAccessibilityPaneTitle(containerView, "Test Custom Title"));

        // Act: Close the side panel.
        closePanel(tab1);
        waitForContainerViewClose(coordinator);

        // Assert: The accessibility title should be cleared.
        CriteriaHelper.pollUiThread(
                () -> ViewCompat.getAccessibilityPaneTitle(containerView) == null,
                "Accessibility pane title was not cleared on close.");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void showPanel_renderContainer() throws Exception {
        // Arrange.
        var coordinator = getSidePanelContainerCoordinator();

        // Act.
        showPanel(mResponsivePageStation.getTab());
        ViewGroup containerView = waitForContainerViewOpen(coordinator);

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
        ViewGroup containerView = waitForContainerViewOpen(coordinator);

        // Act.
        closePanel(tab);
        waitForContainerViewClose(coordinator);

        // Assert.
        ViewGroup contentContainer = containerView.findViewById(R.id.side_panel_content_container);
        assertNotNull(contentContainer);
        assertEquals(0, contentContainer.getChildCount());
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

    @Test
    @MediumTest
    public void changeTheme_retainsOpenPanel() {
        // Arrange:
        var coordinator = getSidePanelContainerCoordinator();
        showPanel(mResponsivePageStation.getTab());
        waitForContainerViewOpen(coordinator);

        // Act: Change the theme.
        ChromeTabbedActivity activityInNewTheme = changeTheme(mResponsivePageStation.getActivity());

        // Assert:
        // (1) Wait for the SidePanelContainerCoordinator in the new Activity to be initialized,
        // then
        // (2) Verify the side panel is still open.
        var newCoordinator = waitForSidePanelContainerCoordinator(activityInNewTheme);
        waitForContainerViewOpen(newCoordinator);
    }

    @Test
    @MediumTest
    public void changeTheme_retainsBrowserControlContainerWidth() {
        // Arrange:
        ChromeTabbedActivity activity = mResponsivePageStation.getActivity();
        var tab = mResponsivePageStation.getTab();
        var coordinator = getSidePanelContainerCoordinator();
        showPanel(tab);
        int sidePanelWidth = waitForContainerViewOpen(coordinator).getWidth();
        CriteriaHelper.pollUiThread(
                () -> {
                    int controlContainerWidth = getBrowserControlContainer(activity).getWidth();
                    return controlContainerWidth > 0
                            && controlContainerWidth + sidePanelWidth <= getWindowWidth(activity);
                },
                "Browser control container isn't resized to accommodate side panel.");

        // Act: Change the theme.
        ChromeTabbedActivity activityInNewTheme = changeTheme(activity);

        // Assert:
        // (1) Wait for the SidePanelContainerCoordinator in the new Activity to be initialized,
        // then
        // (2) Verify the browser control container's width in the new Activity matches the width
        // before theme change.
        var newCoordinator = waitForSidePanelContainerCoordinator(activityInNewTheme);
        int newSidePanelWidth = waitForContainerViewOpen(newCoordinator).getWidth();
        CriteriaHelper.pollUiThread(
                () -> {
                    int newControlContainerWidth =
                            getBrowserControlContainer(activityInNewTheme).getWidth();
                    return newControlContainerWidth > 0
                            && newControlContainerWidth + newSidePanelWidth
                                    <= getWindowWidth(activityInNewTheme);
                },
                "Browser control container isn't resized to accommodate side panel.");
    }

    @Test
    @MediumTest
    public void changeTheme_retainsWebContentsWidth() {
        // Arrange:
        ChromeTabbedActivity activity = mResponsivePageStation.getActivity();
        var tab = mResponsivePageStation.getTab();
        var webContents = tab.getWebContents();
        assertNotNull(webContents);

        var coordinator = getSidePanelContainerCoordinator();
        showPanel(tab);
        int sidePanelWidth = waitForContainerViewOpen(coordinator).getWidth();

        CriteriaHelper.pollUiThread(
                () -> webContents.getWidth() + sidePanelWidth <= getWindowWidth(activity),
                "WebContents isn't resized to accommodate side panel.");

        // Act: Change the theme.
        ChromeTabbedActivity activityInNewTheme = changeTheme(activity);

        // Assert:
        // (1) Wait for the SidePanelContainerCoordinator in the new Activity to be initialized,
        // then
        // (2) Verify the WebContents's width in the new Activity matches the width before theme
        // change.
        var newCoordinator = waitForSidePanelContainerCoordinator(activityInNewTheme);
        int newSidePanelWidth = waitForContainerViewOpen(newCoordinator).getWidth();
        CriteriaHelper.pollUiThread(
                () -> {
                    var newTab = activityInNewTheme.getActivityTabProvider().get();
                    if (newTab == null) {
                        return false;
                    }

                    var newWebContents = newTab.getWebContents();
                    if (newWebContents == null) {
                        return false;
                    }

                    return newWebContents.getWidth() + newSidePanelWidth
                            <= getWindowWidth(activityInNewTheme);
                },
                "WebContents isn't resized to accommodate side panel.");
    }

    @Test
    @MediumTest
    public void changeTheme_retainsNativePageWidth() {
        // Arrange: Open a native page (Bookmarks).
        ChromeTabbedActivity activity = mResponsivePageStation.getActivity();
        var bookmarksNativePageStation =
                mResponsivePageStation.loadPageProgrammatically(
                        UrlConstantResolver.getOriginalBookmarksUrl(),
                        WebPageStation.newBuilder().withExpectedUrlSubstring("bookmarks"));
        var tab = bookmarksNativePageStation.getTab();
        View nativePageView = tab.getView();
        assertNotNull(nativePageView);

        var coordinator = getSidePanelContainerCoordinator();
        showPanel(tab);
        int sidePanelWidth = waitForContainerViewOpen(coordinator).getWidth();
        CriteriaHelper.pollUiThread(
                () -> nativePageView.getWidth() + sidePanelWidth <= getWindowWidth(activity),
                "Native page isn't resized to accommodate side panel.");

        // Act: Change the theme.
        ChromeTabbedActivity activityInNewTheme = changeTheme(activity);

        // Assert:
        // (1) Wait for the SidePanelContainerCoordinator in the new Activity to be initialized,
        // then
        // (2) Verify the native page view's width in the new Activity matches the width before
        // theme change.
        var newCoordinator = waitForSidePanelContainerCoordinator(activityInNewTheme);
        int newSidePanelWidth = waitForContainerViewOpen(newCoordinator).getWidth();
        CriteriaHelper.pollUiThread(
                () -> {
                    var newTab = activityInNewTheme.getActivityTabProvider().get();
                    if (newTab == null) {
                        return false;
                    }

                    View newNativePageView = newTab.getView();
                    if (newNativePageView == null) {
                        return false;
                    }

                    return newNativePageView.getWidth() + newSidePanelWidth
                            <= getWindowWidth(activityInNewTheme);
                },
                "Native page isn't resized to accommodate side panel.");
    }

    @Test
    @MediumTest
    public void closeAllTabsInGridTabSwitcher_closesSidePanel() {
        // Arrange: Open 2 tabs.
        var tab1 = mResponsivePageStation.getTab();
        var newTabPageStation = mResponsivePageStation.openNewTabFast();
        var tab2 = newTabPageStation.getTab();

        // Arrange: Open the side panel for each tab.
        var coordinator = getSidePanelContainerCoordinator();
        showPanel(tab2);
        waitForContainerViewOpen(coordinator);
        mResponsivePageStation = newTabPageStation.selectTabFast(tab1, WebPageStation::newBuilder);
        showPanel(tab1);
        waitForContainerViewOpen(coordinator);

        // Act: Go to the grid tab switcher (GTS), and use the three-dot menu to close all tabs.
        var tabSwitcherStation = mResponsivePageStation.openRegularTabSwitcher();
        var dialogFacility = tabSwitcherStation.openAppMenu().clickCloseAllTabs();
        dialogFacility.positiveButtonElement.clickTo().exitFacility();

        // Act: Stay in GTS, use the "+" button to create a new tab.
        tabSwitcherStation.openNewTab();

        // Assert: The side panel is not shown.
        waitForContainerViewClose(coordinator);
    }

    @Test
    @MediumTest
    public void closePanel_switchTabMidAnimation_staysClosedWhenReturningToTab() {
        // Arrange: Open 2 tabs and show the side panel on tab1.
        Tab tab1 = mResponsivePageStation.getTab();
        var newTabPageStation = mResponsivePageStation.openNewTabFast();
        Tab tab2 = newTabPageStation.getTab();

        SidePanelContainerCoordinatorImpl coordinator = getSidePanelContainerCoordinator();
        mResponsivePageStation = newTabPageStation.selectTabFast(tab1, WebPageStation::newBuilder);
        showPanel(tab1);
        waitForContainerViewOpen(coordinator);

        // Start animated close on tab1.
        closePanel(tab1, /* suppressAnimations= */ false);

        // Switch to tab2 mid-animation.
        newTabPageStation =
                mResponsivePageStation.selectTabFast(tab2, RegularNewTabPageStation::newBuilder);

        // Wait for the closing animation to complete.
        waitForContainerViewClose(coordinator);

        // Act 3: Switch back to tab1.
        mResponsivePageStation = newTabPageStation.selectTabFast(tab1, WebPageStation::newBuilder);

        // Assert: The panel remains closed on tab1 and does not reopen.
        waitForContainerViewClose(coordinator);
    }

    private SidePanelContainerCoordinatorImpl getSidePanelContainerCoordinator() {
        var sidePanelContainerCoordinator =
                getSidePanelContainerCoordinator(mFreshCtaTransitTestRule.getActivity());
        assertNotNull(sidePanelContainerCoordinator);
        return sidePanelContainerCoordinator;
    }

    private @Nullable SidePanelContainerCoordinatorImpl getSidePanelContainerCoordinator(
            ChromeTabbedActivity activity) {
        var rootUiCoordinator = (TabbedRootUiCoordinator) activity.getRootUiCoordinatorForTesting();
        if (rootUiCoordinator == null) {
            return null;
        }

        var sidePanelContainerCoordinator =
                rootUiCoordinator.getSidePanelContainerCoordinatorForTesting();
        return (SidePanelContainerCoordinatorImpl) sidePanelContainerCoordinator;
    }

    private SidePanelContainerCoordinatorImpl waitForSidePanelContainerCoordinator(
            ChromeTabbedActivity activity) {
        var coordinatorRef = new AtomicReference<SidePanelContainerCoordinatorImpl>();
        CriteriaHelper.pollUiThread(
                () -> {
                    var coordinator = getSidePanelContainerCoordinator(activity);
                    if (coordinator != null) {
                        coordinatorRef.set(coordinator);
                    }
                    return coordinator != null;
                },
                "SidePanelContainerCoordinator isn't initialized for the Activity.");
        SidePanelContainerCoordinatorImpl coordinator = coordinatorRef.get();
        assertNotNull(coordinator);
        return coordinator;
    }

    private static void showPanel(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        SidePanelContainerCoordinatorIntegrationTestSupport.showSidePanel(
                                tab, /* suppressAnimations= */ true));
    }

    private static void closePanel(Tab tab) {
        closePanel(tab, /* suppressAnimations= */ true);
    }

    private static void closePanel(Tab tab, boolean suppressAnimations) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        SidePanelContainerCoordinatorIntegrationTestSupport.closeSidePanel(
                                tab, suppressAnimations));
    }

    /**
     * Changes the theme for the {@code currentActivity}.
     *
     * @param currentActivity The current {@link ChromeTabbedActivity}.
     * @return A {@link ChromeTabbedActivity} with the new theme.
     */
    private static ChromeTabbedActivity changeTheme(ChromeTabbedActivity currentActivity) {
        boolean isNightMode = ColorUtils.inNightMode(currentActivity);
        ChromeTabbedActivity activityInNewTheme =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () ->
                                ChromeNightModeTestUtils.setUpNightModeForChromeActivity(
                                        !isNightMode));
        assertNotEquals(isNightMode, ColorUtils.inNightMode(activityInNewTheme));
        return activityInNewTheme;
    }

    /**
     * Waits for the View of {@link SidePanelContainerCoordinator} to have non-zero width.
     *
     * @return The View as returned by {@link SidePanelContainerCoordinatorImpl#getView()}.
     */
    private static ViewGroup waitForContainerViewOpen(
            SidePanelContainerCoordinatorImpl coordinator) {
        View containerView = ThreadUtils.runOnUiThreadBlocking(coordinator::getView);
        assertTrue(containerView instanceof ViewGroup);

        CriteriaHelper.pollUiThread(
                () -> containerView.getWidth() > 0,
                "The container View should have been attached and laid out.");
        return (ViewGroup) containerView;
    }

    /** Waits for the View of {@link SidePanelContainerCoordinator} to be detached. */
    private static void waitForContainerViewClose(SidePanelContainerCoordinatorImpl coordinator) {
        View containerView = ThreadUtils.runOnUiThreadBlocking(coordinator::getView);

        CriteriaHelper.pollUiThread(
                () -> containerView.getParent() == null,
                "The container View should have been detached.");
    }

    private static View getBrowserControlContainer(Activity activity) {
        View browserControlContainer =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> activity.findViewById(org.chromium.chrome.R.id.control_container));
        assertNotNull(browserControlContainer);
        return browserControlContainer;
    }

    private static @Px int getWindowWidth(Activity activity) {
        return WindowMetricsCalculator.getOrCreate()
                .computeCurrentWindowMetrics(activity)
                .getBounds()
                .width();
    }
}
