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

import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.Transition.Trigger;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.toolbar.home_button.HomeButton;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.layouts.LayoutTypeVisibleCondition;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.function.Function;

/**
 * The screen that shows a web or native page with the toolbar within a tab.
 *
 * <p>Contains extra configurable Conditions such as waiting for a tab to be created, selected, have
 * the expected title, etc.
 */
public class PageStation extends BasePageStation<ChromeTabbedActivity> {

    /**
     * Builder for all PageStation subclasses.
     *
     * @param <T> the subclass of PageStation to build.
     */
    public static class Builder<T extends PageStation>
            extends BasePageStation.Builder<ChromeTabbedActivity, T, Builder<T>> {
        public Builder(Function<Builder<T>, T> factoryMethod) {
            super(factoryMethod);
        }
    }

    public static final ViewSpec<UrlBar> URL_BAR = viewSpec(UrlBar.class, withId(R.id.url_bar));
    public ViewElement<ToolbarControlContainer> toolbarElement;
    public ViewElement<ToggleTabStackButton> tabSwitcherButtonElement;
    public ViewElement<ImageButton> menuButtonElement;

    /** Prefer the PageStation's subclass |newBuilder()|. */
    public static Builder<PageStation> newGenericBuilder() {
        return new Builder<>(PageStation::new);
    }

    /** Use the PageStation's subclass |newBuilder()|. */
    protected <T extends PageStation> PageStation(Builder<T> builder) {
        super(ChromeTabbedActivity.class, builder);

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
        declareView(HomeButton.class, withId(R.id.home_button), ViewElement.unscopedOption());
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
        return enterFacilitySync(
                new TabSwitcherActionMenuFacility(),
                tabSwitcherButtonElement.getLongPressTrigger());
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public PageAppMenuFacility<PageStation> openGenericAppMenu() {
        recheckActiveConditions();

        return enterFacilitySync(
                new PageAppMenuFacility<PageStation>(), menuButtonElement.getClickTrigger());
    }

    /** Shortcut to open a new tab programmatically as if selecting "New Tab" from the app menu. */
    public RegularNewTabPageStation openNewTabFast() {
        return travelToSync(
                RegularNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build(),
                () ->
                        MenuUtils.invokeCustomMenuActionSync(
                                InstrumentationRegistry.getInstrumentation(),
                                getActivity(),
                                R.id.new_tab_menu_id));
    }

    /**
     * Shortcut to open a new incognito tab programmatically as if selecting "New Incognito Tab"
     * from the app menu.
     */
    public IncognitoNewTabPageStation openNewIncognitoTabFast() {
        return travelToSync(
                IncognitoNewTabPageStation.newBuilder()
                        .withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .build(),
                () ->
                        MenuUtils.invokeCustomMenuActionSync(
                                InstrumentationRegistry.getInstrumentation(),
                                getActivity(),
                                R.id.new_incognito_tab_menu_id));
    }

    /** Shortcut to select a different tab programmatically. */
    public <T extends PageStation> T selectTabFast(
            Tab tabToSelect, Supplier<Builder<T>> pageStationFactory) {
        return travelToSync(
                pageStationFactory
                        .get()
                        .withIncognito(tabToSelect.isIncognitoBranded())
                        .withIsOpeningTabs(0)
                        .withIsSelectingTabs(1)
                        .build(),
                Transition.runTriggerOnUiThreadOption(),
                () ->
                        TabModelUtils.selectTabById(
                                getActivity().getTabModelSelector(),
                                tabToSelect.getId(),
                                TabSelectionType.FROM_USER));
    }

    /** Opens the tab switcher by pressing the toolbar tab switcher button. */
    public RegularTabSwitcherStation openRegularTabSwitcher() {
        assert !mIncognito;
        return travelToSync(
                RegularTabSwitcherStation.from(getActivity().getTabModelSelector()),
                tabSwitcherButtonElement.getClickTrigger());
    }

    /** Opens the incognito tab switcher by pressing the toolbar tab switcher button. */
    public IncognitoTabSwitcherStation openIncognitoTabSwitcher() {
        assert mIncognito;
        return travelToSync(
                IncognitoTabSwitcherStation.from(getActivity().getTabModelSelector()),
                tabSwitcherButtonElement.getClickTrigger());
    }

    /** Loads a |url| leading to a web page in the same tab and waits to transition. */
    public WebPageStation loadWebPageProgrammatically(String url) {
        return loadPageProgrammatically(url, WebPageStation.newBuilder());
    }

    public WebPageStation loadAboutBlank() {
        return loadWebPageProgrammatically("about:blank");
    }

    /** Loads a |url| in another tab as if a link was clicked and waits to transition. */
    public <T extends PageStation> T openFakeLink(String url, Builder<T> builder) {
        return travelToSync(
                builder.withIsOpeningTabs(1)
                        .withIsSelectingTabs(1)
                        .withIncognito(mIncognito)
                        .withExpectedUrlSubstring(url)
                        .build(),
                Transition.runTriggerOnUiThreadOption(),
                () ->
                        getActivity()
                                .getTabCreator(mIncognito)
                                .launchUrl(url, TabLaunchType.FROM_LINK));
    }

    /**
     * Loads a |url| in another tab leading to a webpage as if a link was clicked and waits to
     * transition.
     */
    public WebPageStation openFakeLinkToWebPage(String url) {
        return openFakeLink(url, WebPageStation.newBuilder());
    }

    /** Move to next tab by swiping the toolbar left. */
    public <T extends PageStation> T swipeToolbarToNextTab(
            PageStation.Builder<T> destinationBuilder) {
        return swipeToolbar(destinationBuilder, /* directionRight= */ false);
    }

    /** Move to previous tab by swiping the toolbar right. */
    public <T extends PageStation> T swipeToolbarToPreviousTab(
            PageStation.Builder<T> destinationBuilder) {
        return swipeToolbar(destinationBuilder, /* directionRight= */ true);
    }

    public <T extends PageStation> T swipeToolbar(
            PageStation.Builder<T> destinationBuilder, boolean directionRight) {
        ToolbarSwipeCoordinates coords =
                new ToolbarSwipeCoordinates(toolbarElement.get(), directionRight);

        T destination = destinationBuilder.initFrom(this).withIsSelectingTabs(1).build();
        return travelToSync(
                destination,
                () ->
                        TouchCommon.performDrag(
                                toolbarElement.get(),
                                coords.mFromX,
                                coords.mToX,
                                coords.mY,
                                coords.mY,
                                ToolbarSwipeCoordinates.STEP_COUNT,
                                500));
    }

    /** Start moving to next tab by swiping the toolbar left, but do not finish the swipe. */
    public SwipingToTabFacility swipeToolbarToNextTabPartial() {
        return swipeToolbarPartial(/* directionRight= */ false);
    }

    /** Start moving to previous tab by swiping the toolbar right, but do not finish the swipe. */
    public SwipingToTabFacility swipeToolbarToPreviousTabPartial() {
        return swipeToolbarPartial(/* directionRight= */ true);
    }

    private SwipingToTabFacility swipeToolbarPartial(boolean directionRight) {
        ToolbarSwipeCoordinates coords =
                new ToolbarSwipeCoordinates(toolbarElement.get(), directionRight);
        long downTime = SystemClock.uptimeMillis();
        Activity activity = getActivity();

        Trigger firstPartTrigger =
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
        Trigger secondPartTrigger =
                () -> {
                    TouchCommon.dragEnd(activity, coords.mToX, coords.mY, downTime);
                };
        return enterFacilitySync(new SwipingToTabFacility(secondPartTrigger), firstPartTrigger);
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
