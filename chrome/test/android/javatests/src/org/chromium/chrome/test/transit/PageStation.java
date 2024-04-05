// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewElement.unscopedViewElement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.StationFacility;
import org.chromium.base.test.transit.TransitStation;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.PageTransition;

import java.util.function.Function;

/**
 * The screen that shows a loaded webpage with the omnibox and the toolbar.
 *
 * <p>Contains extra configurable Conditions such as waiting for a tab to be created, selected, have
 * the expected title, etc.
 */
public class PageStation extends TransitStation {

    /**
     * Builder for all PageStation subclasses.
     *
     * @param <T> the subclass of PageStation to build.
     */
    public static class Builder<T extends PageStation> {
        private final Function<Builder<T>, T> mFactoryMethod;
        private ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
        private boolean mIncognito;
        private Boolean mIsOpeningTab;
        private Boolean mIsSelectingTab;
        private String mPath;
        private String mTitle;

        public Builder(Function<Builder<T>, T> factoryMethod) {
            mFactoryMethod = factoryMethod;
        }

        public Builder<T> withActivityTestRule(ChromeTabbedActivityTestRule activityTestRule) {
            mChromeTabbedActivityTestRule = activityTestRule;
            return this;
        }

        public Builder<T> withIncognito(boolean incognito) {
            mIncognito = incognito;
            return this;
        }

        public Builder<T> withIsOpeningTab(boolean isOpeningTab) {
            mIsOpeningTab = isOpeningTab;
            return this;
        }

        public Builder<T> withIsSelectingTab(boolean isSelectingTab) {
            mIsSelectingTab = isSelectingTab;
            return this;
        }

        public Builder<T> withPath(String path) {
            mPath = path;
            return this;
        }

        public Builder<T> withTitle(String title) {
            mTitle = title;
            return this;
        }

        public Builder<T> initFrom(PageStation previousStation) {
            mChromeTabbedActivityTestRule = previousStation.getTestRule();
            mIncognito = previousStation.isIncognito();
            return this;
        }

        public T build() {
            return mFactoryMethod.apply(this);
        }
    }

    protected final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
    protected final boolean mIncognito;
    protected final boolean mIsOpeningTab;
    protected final boolean mIsSelectingTab;
    protected final String mPath;
    protected final String mTitle;

    // TODO(crbug.com/1524512): This should be owned, but the tab_switcher_button exists in the
    // tab switcher, even though the tab switcher's toolbar is drawn over it.
    public static final ViewElement TAB_SWITCHER_BUTTON =
            unscopedViewElement(withId(R.id.tab_switcher_button));
    public static final ViewElement MENU_BUTTON =
            unscopedViewElement(withId(R.id.menu_button_wrapper));
    public static final ViewElement MENU_BUTTON2 = unscopedViewElement(withId(R.id.menu_button));

    protected PageStationTabModelObserver mTabModelObserver;
    protected PageLoadedCondition mPageLoadedEnterCondition;

    /** Use {@link #newPageStationBuilder()} or the PageStation's subclass |newBuilder()|. */
    protected <T extends PageStation> PageStation(Builder<T> builder) {
        // activityTestRule is required
        assert builder.mChromeTabbedActivityTestRule != null;
        mChromeTabbedActivityTestRule = builder.mChromeTabbedActivityTestRule;

        // incognito is optional and defaults to false
        mIncognito = builder.mIncognito;

        // isOpeningTab is required
        assert builder.mIsOpeningTab != null;
        mIsOpeningTab = builder.mIsOpeningTab;

        // isSelectingTab is required
        assert builder.mIsSelectingTab != null;
        mIsSelectingTab = builder.mIsSelectingTab;

        // path is optional
        mPath = builder.mPath;

        // title is optional
        mTitle = builder.mTitle;
    }

    /**
     * Get a new Builder for the base PageStation class.
     *
     * <p>If you're building a subclass, get a new Builder from DerivedPageStation#newBuilder()
     * instead.
     */
    public static Builder<PageStation> newPageStationBuilder() {
        return new Builder<>(PageStation::new);
    }

    @Override
    protected void onStartMonitoringTransitionTo() {
        if (mIsOpeningTab || mIsSelectingTab) {
            mTabModelObserver = new PageStationTabModelObserver();
            mTabModelObserver.install();
        }
    }

    @Override
    protected void onStopMonitoringTransitionTo() {
        if (mTabModelObserver != null) {
            mTabModelObserver.uninstall();
            mTabModelObserver = null;
        }
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(TAB_SWITCHER_BUTTON);
        elements.declareView(MENU_BUTTON);
        elements.declareView(MENU_BUTTON2);

        if (mIsOpeningTab) {
            elements.declareEnterCondition(
                    CallbackCondition.instrumentationThread(
                            mTabModelObserver.mTabAddedCallback, "Receive tab opened callback"));
        }
        if (mIsSelectingTab) {
            elements.declareEnterCondition(
                    CallbackCondition.instrumentationThread(
                            mTabModelObserver.mTabSelectedCallback,
                            "Receive tab selected callback"));
        }

        mPageLoadedEnterCondition =
                new PageLoadedCondition(mChromeTabbedActivityTestRule, mIncognito);
        elements.declareEnterCondition(mPageLoadedEnterCondition);
        elements.declareEnterCondition(
                new PageInteractableOrHiddenCondition(mPageLoadedEnterCondition));

        if (mTitle != null) {
            elements.declareEnterCondition(new PageTitleCondition(mTitle));
        }
        if (mPath != null) {
            elements.declareEnterCondition(new PageUrlContainsCondition(mPath));
        }
    }

    public ChromeTabbedActivityTestRule getTestRule() {
        return mChromeTabbedActivityTestRule;
    }

    public boolean isIncognito() {
        return mIncognito;
    }

    private class PageStationTabModelObserver implements TabModelObserver {
        private CallbackHelper mTabAddedCallback = new CallbackHelper();
        private CallbackHelper mTabSelectedCallback = new CallbackHelper();
        private TabModel mTabModel;

        @Override
        public void didSelectTab(Tab tab, int type, int lastId) {
            mTabSelectedCallback.notifyCalled();
        }

        @Override
        public void didAddTab(Tab tab, int type, int creationState, boolean markedForSelection) {
            mTabAddedCallback.notifyCalled();
        }

        public void install() {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabModel =
                                getTestRule()
                                        .getActivity()
                                        .getTabModelSelector()
                                        .getModel(isIncognito());
                        mTabModel.addObserver(mTabModelObserver);
                    });
        }

        private void uninstall() {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabModel.removeObserver(mTabModelObserver);
                    });
        }
    }

    /** Condition to check the page title. */
    public class PageTitleCondition extends Condition {

        private final String mExpectedTitle;

        public PageTitleCondition(String expectedTitle) {
            super(/* isRunOnUiThread= */ true);
            mExpectedTitle = expectedTitle;
        }

        @Override
        public ConditionStatus check() throws Exception {
            Tab tab = getTestRule().getActivity().getActivityTab();
            if (tab == null) {
                return notFulfilled("null ActivityTab");
            }
            String title = tab.getTitle();
            return whether(mExpectedTitle.equals(title), "ActivityTab title: \"%s\"", title);
        }

        @Override
        public String buildDescription() {
            return String.format("Title of activity tab is \"%s\"", mExpectedTitle);
        }
    }

    /** Condition to check the page url contains a certain substring. */
    public class PageUrlContainsCondition extends Condition {

        private final String mExpectedUrlPiece;

        public PageUrlContainsCondition(String expectedUrl) {
            super(/* isRunOnUiThread= */ true);
            mExpectedUrlPiece = expectedUrl;
        }

        @Override
        public ConditionStatus check() throws Exception {
            Tab tab = getTestRule().getActivity().getActivityTab();
            if (tab == null) {
                return notFulfilled("null ActivityTab");
            }
            String url = tab.getUrl().getSpec();
            return whether(url.contains(mExpectedUrlPiece), "ActivityTab url: \"%s\"", url);
        }

        @Override
        public String buildDescription() {
            return String.format("URL of activity tab contains \"%s\"", mExpectedUrlPiece);
        }
    }

    /** Long presses the tab switcher button to open the action menu. */
    public TabSwitcherActionMenuFacility openTabSwitcherActionMenu() {
        recheckActiveConditions();

        TabSwitcherActionMenuFacility menu = new TabSwitcherActionMenuFacility(this);
        return StationFacility.enterSync(menu, () -> TAB_SWITCHER_BUTTON.perform(longClick()));
    }

    public PageAppMenuFacility openAppMenu() {
        recheckActiveConditions();

        PageAppMenuFacility menu = new PageAppMenuFacility(this);

        return StationFacility.enterSync(menu, () -> MENU_BUTTON2.perform(click()));
    }

    /** Opens the tab switcher by pressing the toolbar tab switcher button. */
    public <T extends TabSwitcherStation> T openTabSwitcher(Class<T> expectedDestination) {
        recheckActiveConditions();

        T destination;
        if (isIncognito()) {
            destination =
                    expectedDestination.cast(
                            new IncognitoTabSwitcherStation(mChromeTabbedActivityTestRule));
        } else {
            destination =
                    expectedDestination.cast(
                            new RegularTabSwitcherStation(mChromeTabbedActivityTestRule));
        }
        return Trip.travelSync(this, destination, () -> TAB_SWITCHER_BUTTON.perform(click()));
    }

    /** Opens the hub by pressing the toolbar tab switcher button. */
    public <T extends HubBaseStation> T openHub(Class<T> expectedDestination) {
        recheckActiveConditions();

        T destination =
                expectedDestination.cast(
                        HubStationUtils.createHubStation(
                                isIncognito() ? PaneId.INCOGNITO_TAB_SWITCHER : PaneId.TAB_SWITCHER,
                                getTestRule()));

        return Trip.travelSync(this, destination, () -> TAB_SWITCHER_BUTTON.perform(click()));
    }

    /** Loads a |url| in the same tab and waits to transition to the given |destination|. */
    public <T extends PageStation> T loadPageProgramatically(
            Builder<T> destinationBuilder, String url) {
        T destination =
                destinationBuilder
                        .initFrom(this)
                        .withIsOpeningTab(false)
                        .withIsSelectingTab(false)
                        .build();

        return loadPageProgramatically(destination, url);
    }

    /** Loads a |url| in the same tab and waits to transition to the given |destination|. */
    public <T extends PageStation> T loadPageProgramatically(T destination, String url) {
        Runnable r =
                () -> {
                    @PageTransition
                    int transitionType = PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR;
                    getTestRule()
                            .getActivity()
                            .getActivityTab()
                            .loadUrl(new LoadUrlParams(url, transitionType));
                };
        return Trip.travelSync(
                this,
                destination,
                Transition.timeoutOption(10000),
                () -> ThreadUtils.runOnUiThread(r));
    }
}
