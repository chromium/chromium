// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.app.Activity;
import android.os.SystemClock;
import android.view.View;
import android.widget.ImageButton;

import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.test.transit.ChromeTriggers;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.layouts.LayoutTypeVisibleCondition;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.function.Supplier;

/**
 * The screen that shows a web or native page with the toolbar within a tab.
 *
 * <p>Contains extra configurable Conditions such as waiting for a tab to be created, selected, have
 * the expected title, etc.
 */
public class CtaPageStation extends BasePageStation<ChromeTabbedActivity> {
    public static final ViewSpec<UrlBar> URL_BAR = viewSpec(UrlBar.class, withId(R.id.url_bar));
    public ViewElement<ToolbarControlContainer> toolbarElement;
    public ViewElement<ToggleTabStackButton> tabSwitcherButtonElement;
    public ViewElement<ImageButton> menuButtonElement;

    /** Prefer the CtaPageStation's subclass |newBuilder()|. */
    public static Builder<CtaPageStation> newGenericBuilder() {
        return new Builder<>(CtaPageStation::new);
    }

    /** Use the CtaPageStation's subclass |newBuilder()|. */
    protected CtaPageStation(Config config) {
        super(ChromeTabbedActivity.class, config);

        declareEnterCondition(
                new LayoutTypeVisibleCondition(mActivityElement, LayoutType.BROWSING));

        // TODO(crbug.com/41497463): These should be scoped, but for now they need to be unscoped
        // since they unintentionally still exist in the non-Hub tab switcher. They are mostly
        // occluded by the tab switcher toolbar, but at least the tab_switcher_button is still
        // visible.
        toolbarElement =
                declareView(
                        ToolbarControlContainer.class,
                        withId(R.id.control_container),
                        ViewElement.unscopedOption());
        // TODO(crbug.com/416324280): Declare the HomeButton with R.id.home_button as an optional
        //  ViewElement.
        tabSwitcherButtonElement =
                declareView(
                        ToggleTabStackButton.class,
                        withId(R.id.tab_switcher_button),
                        ViewElement.unscopedOption());
        menuButtonElement =
                declareView(
                        ImageButton.class, withId(R.id.menu_button), ViewElement.unscopedOption());
    }

    /** Long presses the tab switcher button to open the action menu. */
    public TabSwitcherActionMenuFacility openTabSwitcherActionMenu() {
        recheckActiveConditions();
        return tabSwitcherButtonElement
                .longPressTo()
                .enterFacility(new TabSwitcherActionMenuFacility());
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public PageAppMenuFacility<CtaPageStation> openGenericAppMenu() {
        recheckActiveConditions();

        return menuButtonElement.clickTo().enterFacility(new PageAppMenuFacility<>());
    }

    /** Shortcut to open a new tab programmatically as if selecting "New Tab" from the app menu. */
    public RegularNewTabPageStation openNewTabFast() {
        return ChromeTriggers.invokeCustomMenuActionTo(R.id.new_tab_menu_id, this)
                .arriveAt(RegularNewTabPageStation.newBuilder().initOpeningNewTab().build());
    }

    /**
     * Shortcut to open a new incognito tab programmatically as if selecting "New Incognito Tab"
     * from the app menu.
     */
    public IncognitoNewTabPageStation openNewIncognitoTabFast() {
        assert mIsIncognito || !IncognitoUtils.shouldOpenIncognitoAsWindow()
                : "Incognito tabs can only be opened in incognito windows.";
        return ChromeTriggers.invokeCustomMenuActionTo(R.id.new_incognito_tab_menu_id, this)
                .arriveAt(IncognitoNewTabPageStation.newBuilder().initOpeningNewTab().build());
    }

    /**
     * Shortcut to open a new window programmatically as if selecting "New Window" from the app
     * menu.
     */
    public RegularNewTabPageStation openNewWindowFast() {
        return ChromeTriggers.invokeCustomMenuActionTo(R.id.new_window_menu_id, this)
                .inNewTask()
                .arriveAt(RegularNewTabPageStation.newBuilder().withEntryPoint().build());
    }

    /**
     * Shortcut to open a new incognito window programmatically as if selecting "New Incognito
     * Window" from the app menu.
     */
    public IncognitoNewTabPageStation openNewIncognitoWindowFast() {
        return ChromeTriggers.invokeCustomMenuActionTo(R.id.new_incognito_window_menu_id, this)
                .inNewTask()
                .arriveAt(IncognitoNewTabPageStation.newBuilder().withEntryPoint().build());
    }

    /**
     * Attempts to open a new tab programmatically as if selecting "New Tab" from the app menu if
     * available. If not available, attempts to open a new window.
     */
    public RegularNewTabPageStation openNewTabOrWindowFast() {
        if (IncognitoUtils.shouldOpenIncognitoAsWindow() && mIsIncognito) {
            return openNewWindowFast();
        } else {
            return openNewTabFast();
        }
    }

    /**
     * Attempts to open a new incognito tab programmatically as if selecting "New Incognito Tab"
     * from the app menu if available. If not available, attempts to open a new incognito window.
     */
    public IncognitoNewTabPageStation openNewIncognitoTabOrWindowFast() {
        if (IncognitoUtils.shouldOpenIncognitoAsWindow() && !mIsIncognito) {
            return openNewIncognitoWindowFast();
        } else {
            return openNewIncognitoTabFast();
        }
    }

    /** Shortcut to select a different tab programmatically. */
    public <T extends CtaPageStation> T selectTabFast(
            Tab tabToSelect, Supplier<Builder<T>> pageStationFactory) {
        return runOnUiThreadTo(
                        () ->
                                TabModelUtils.selectTabById(
                                        getTabModelSelector(),
                                        tabToSelect.getId(),
                                        TabSelectionType.FROM_USER))
                .arriveAt(
                        pageStationFactory
                                .get()
                                .withIncognito(tabToSelect.isIncognitoBranded())
                                .initSelectingExistingTab()
                                .build());
    }

    /** Opens the tab switcher by pressing the toolbar tab switcher button. */
    public RegularTabSwitcherStation openRegularTabSwitcher() {
        return openRegularTabSwitcherAnd().completeAndGet(RegularTabSwitcherStation.class);
    }

    /** Start a Trip to open the tab switcher by pressing the toolbar tab switcher button. */
    public TripBuilder openRegularTabSwitcherAnd() {
        assert !mIsIncognito;
        return tabSwitcherButtonElement
                .clickTo()
                .arriveAtAnd(RegularTabSwitcherStation.from(getTabModelSelector()));
    }

    /** Opens the incognito tab switcher by pressing the toolbar tab switcher button. */
    public IncognitoTabSwitcherStation openIncognitoTabSwitcher() {
        return openIncognitoTabSwitcherAnd().completeAndGet(IncognitoTabSwitcherStation.class);
    }

    /**
     * Start a Trip to open the incognito tab switcher by pressing the toolbar tab switcher button.
     */
    public TripBuilder openIncognitoTabSwitcherAnd() {
        assert mIsIncognito;
        return tabSwitcherButtonElement
                .clickTo()
                .arriveAtAnd(IncognitoTabSwitcherStation.from(getTabModelSelector()));
    }

    /** Loads a |url| leading to a web page in the same tab and waits to transition. */
    public WebPageStation loadWebPageProgrammatically(String url) {
        return loadPageProgrammatically(url, WebPageStation.newBuilder());
    }

    public WebPageStation loadAboutBlank() {
        return loadWebPageProgrammatically("about:blank");
    }

    /** Loads a |url| in another tab as if a link was clicked and waits to transition. */
    public <T extends CtaPageStation> T openFakeLink(String url, Builder<T> builder) {
        return runOnUiThreadTo(
                        () ->
                                getActivity()
                                        .getTabCreator(mIsIncognito)
                                        .launchUrl(url, TabLaunchType.FROM_LINK))
                .arriveAt(
                        builder.initOpeningNewTab()
                                .withIncognito(mIsIncognito)
                                .withExpectedUrlSubstring(url)
                                .build());
    }

    /**
     * Loads a |url| in another tab leading to a webpage as if a link was clicked and waits to
     * transition.
     */
    public WebPageStation openFakeLinkToWebPage(String url) {
        return openFakeLink(url, WebPageStation.newBuilder());
    }

    /** Move to next tab by swiping the toolbar left. */
    public TripBuilder swipeToolbarToNextTabTo() {
        return swipeToolbarTo(/* directionRight= */ false);
    }

    /** Move to previous tab by swiping the toolbar right. */
    public TripBuilder swipeToolbarToPreviousTabTo() {
        return swipeToolbarTo(/* directionRight= */ true);
    }

    /** Move to previous tab by swiping the toolbar right. */
    public WebPageStation swipeToolbarToPreviousTab(WebPageStation pageStation) {
        return pageStation
                .swipeToolbarToPreviousTabTo()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .initFrom(pageStation)
                                .initSelectingExistingTab()
                                .build());
    }

    /** Move to next tab by swiping the toolbar left. */
    public WebPageStation swipeToolbarToNextTab(WebPageStation pageStation) {
        return pageStation
                .swipeToolbarToNextTabTo()
                .arriveAt(
                        WebPageStation.newBuilder()
                                .initFrom(pageStation)
                                .initSelectingExistingTab()
                                .build());
    }

    public TripBuilder swipeToolbarTo(boolean directionRight) {
        ToolbarSwipeCoordinates coords =
                new ToolbarSwipeCoordinates(toolbarElement.value(), directionRight);

        return runTo(
                        () ->
                                TouchCommon.performDrag(
                                        toolbarElement.value(),
                                        coords.mFromX,
                                        coords.mToX,
                                        coords.mY,
                                        coords.mY,
                                        ToolbarSwipeCoordinates.STEP_COUNT,
                                        500))
                .withPossiblyAlreadyFulfilled();
    }

    /** Start moving to previous tab by swiping the toolbar right, but do not finish the swipe. */
    public SwipingToTabFacility swipeToolbarToPreviousTabPartial() {
        return swipeToolbarPartial(/* directionRight= */ true);
    }

    private SwipingToTabFacility swipeToolbarPartial(boolean directionRight) {
        ToolbarSwipeCoordinates coords =
                new ToolbarSwipeCoordinates(toolbarElement.value(), directionRight);
        long downTime = SystemClock.uptimeMillis();
        Activity activity = getActivity();

        Runnable firstPartTrigger =
                () -> {
                    TouchCommon.dragStart(activity, coords.mFromX, coords.mY, downTime);
                    TouchCommon.dragTo(
                            activity,
                            coords.mFromX,
                            coords.mToX,
                            coords.mY,
                            coords.mY,
                            ToolbarSwipeCoordinates.STEP_COUNT,
                            downTime);
                };
        Runnable secondPartTrigger =
                () -> {
                    TouchCommon.dragEnd(activity, coords.mToX, coords.mY, downTime);
                };
        return runTo(firstPartTrigger).enterFacility(new SwipingToTabFacility(secondPartTrigger));
    }

    private static class ToolbarSwipeCoordinates {
        static final int STEP_COUNT = 25;
        final int mFromX;
        final int mToX;
        final int mY;

        private ToolbarSwipeCoordinates(View toolbar, boolean directionRight) {
            int[] toolbarPos = new int[2];
            toolbar.getLocationOnScreen(toolbarPos);
            final int width = toolbar.getWidth();
            final int height = toolbar.getHeight();

            this.mFromX = toolbarPos[0] + width / 2;
            this.mToX = toolbarPos[0] + (directionRight ? width : 0);
            this.mY = toolbarPos[1] + height / 2;
        }
    }
}
