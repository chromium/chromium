// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.fail;

import static org.chromium.base.test.transit.ViewElement.unscopedViewElement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ActivityElement;
import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.InstrumentationThreadCondition;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.Trip;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.PageTransition;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;

/**
 * The screen that shows a web or native page with the toolbar within a tab.
 *
 * <p>Contains extra configurable Conditions such as waiting for a tab to be created, selected, have
 * the expected title, etc.
 */
public class PageStation extends Station {
    /**
     * Builder for all PageStation subclasses.
     *
     * @param <T> the subclass of PageStation to build.
     */
    public static class Builder<T extends PageStation> {
        private final Function<Builder<T>, T> mFactoryMethod;
        private ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
        private boolean mIncognito;
        private boolean mIsEntryPoint;
        private Integer mNumTabsBeingOpened;
        private Integer mNumTabsBeingSelected;
        private Tab mTabAlreadySelected;
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

        public Builder<T> withIsOpeningTabs(int numTabsBeingOpened) {
            assert numTabsBeingOpened >= 0;
            mNumTabsBeingOpened = numTabsBeingOpened;
            return this;
        }

        public Builder<T> withTabAlreadySelected(Tab currentTab) {
            mTabAlreadySelected = currentTab;
            mNumTabsBeingSelected = 0;
            return this;
        }

        public Builder<T> withIsSelectingTabs(int numTabsBeingSelected) {
            assert numTabsBeingSelected > 0
                    : "Use withIsSelectingTab() if the PageStation is still in the current tab";
            mNumTabsBeingSelected = numTabsBeingSelected;
            return this;
        }

        public Builder<T> withEntryPoint() {
            mNumTabsBeingOpened = 0;
            mNumTabsBeingSelected = 0;
            mIsEntryPoint = true;
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
    protected final boolean mIsEntryPoint;
    protected final int mNumTabsBeingOpened;
    protected final int mNumTabsBeingSelected;
    protected final Tab mTabAlreadySelected;
    protected final String mPath;
    protected final String mTitle;

    // TODO(crbug.com/41497463): These should be shared, not unscoped, but for now they need to be
    // unscoped since they unintentionally still exist in the non-Hub tab switcher. They are mostly
    // occluded by the tab switcher toolbar, but at least the tab_switcher_button is still visible.
    public static final ViewElement HOME_BUTTON = unscopedViewElement(withId(R.id.home_button));
    public static final ViewElement TAB_SWITCHER_BUTTON =
            unscopedViewElement(withId(R.id.tab_switcher_button));
    public static final ViewElement MENU_BUTTON = unscopedViewElement(withId(R.id.menu_button));

    protected ActivityElement<ChromeTabbedActivity> mActivityElement;
    protected Supplier<Tab> mActivityTabSupplier;
    protected Supplier<Tab> mSelectedTabSupplier;
    protected PageLoadedCondition mPageLoadedCondition;

    /** Use {@link #newPageStationBuilder()} or the PageStation's subclass |newBuilder()|. */
    protected <T extends PageStation> PageStation(Builder<T> builder) {
        // activityTestRule is required
        assert builder.mChromeTabbedActivityTestRule != null;
        mChromeTabbedActivityTestRule = builder.mChromeTabbedActivityTestRule;

        // incognito is optional and defaults to false
        mIncognito = builder.mIncognito;

        // isEntryPoint is optional and defaults to false
        mIsEntryPoint = builder.mIsEntryPoint;

        // isOpeningTab is required
        assert builder.mNumTabsBeingOpened != null;
        mNumTabsBeingOpened = builder.mNumTabsBeingOpened;

        // mNumTabsBeingSelected is required
        assert builder.mNumTabsBeingSelected != null;
        mNumTabsBeingSelected = builder.mNumTabsBeingSelected;

        mTabAlreadySelected = builder.mTabAlreadySelected;

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
    public void declareElements(Elements.Builder elements) {
        mActivityElement = elements.declareActivity(ChromeTabbedActivity.class);
        elements.declareView(HOME_BUTTON);
        elements.declareView(TAB_SWITCHER_BUTTON);
        elements.declareView(MENU_BUTTON);

        if (mNumTabsBeingOpened > 0) {
            elements.declareEnterCondition(new TabAddedCondition(mNumTabsBeingOpened));
        }

        if (mIsEntryPoint) {
            // In entry points we just match the first ActivityTab we see, instead of waiting for
            // callbacks.
            mActivityTabSupplier =
                    elements.declareEnterCondition(new AnyActivityTabCondition(mActivityElement));
        } else {
            if (mNumTabsBeingSelected > 0) {
                // The last tab of N opened is the Tab that mSelectedTabSupplier will supply.
                mSelectedTabSupplier =
                        elements.declareEnterCondition(
                                new TabSelectedCondition(mNumTabsBeingSelected));
            } else {
                // The Tab already created and provided to the constructor is the one that is
                // expected to be the activityTab.
                mSelectedTabSupplier = () -> mTabAlreadySelected;
            }
            // Only returns the tab when it is the activityTab.
            mActivityTabSupplier =
                    elements.declareEnterCondition(
                            new CorrectActivityTabCondition(
                                    mActivityElement, mSelectedTabSupplier));
        }
        mPageLoadedCondition =
                elements.declareEnterCondition(
                        new PageLoadedCondition(mActivityTabSupplier, mIncognito));

        elements.declareEnterCondition(new PageInteractableOrHiddenCondition(mPageLoadedCondition));

        if (mTitle != null) {
            elements.declareEnterCondition(new PageTitleCondition(mTitle, mPageLoadedCondition));
        }
        if (mPath != null) {
            elements.declareEnterCondition(
                    new PageUrlContainsCondition(mPath, mPageLoadedCondition));
        }
    }

    public ChromeTabbedActivityTestRule getTestRule() {
        return mChromeTabbedActivityTestRule;
    }

    public boolean isIncognito() {
        return mIncognito;
    }

    /** Condition to check the page title. */
    public static class PageTitleCondition extends Condition {
        private final String mExpectedTitle;
        private final Supplier<Tab> mLoadedTabSupplier;

        public PageTitleCondition(String expectedTitle, Supplier<Tab> loadedTabSupplier) {
            super(/* isRunOnUiThread= */ true);
            mExpectedTitle = expectedTitle;
            mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws Exception {
            String title = mLoadedTabSupplier.get().getTitle();
            return whether(mExpectedTitle.equals(title), "ActivityTab title: \"%s\"", title);
        }

        @Override
        public String buildDescription() {
            return String.format("Title of activity tab is \"%s\"", mExpectedTitle);
        }
    }

    /** Condition to check the page url contains a certain substring. */
    public static class PageUrlContainsCondition extends Condition {
        private final String mExpectedUrlPiece;
        private final Supplier<Tab> mLoadedTabSupplier;

        public PageUrlContainsCondition(String expectedUrl, Supplier<Tab> loadedTabSupplier) {
            super(/* isRunOnUiThread= */ true);
            mExpectedUrlPiece = expectedUrl;
            mLoadedTabSupplier = dependOnSupplier(loadedTabSupplier, "LoadedTab");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws Exception {
            String url = mLoadedTabSupplier.get().getUrl().getSpec();
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
        return Facility.enterSync(menu, () -> TAB_SWITCHER_BUTTON.perform(longClick()));
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public PageAppMenuFacility<PageStation> openGenericAppMenu() {
        recheckActiveConditions();

        PageAppMenuFacility<PageStation> menu = new PageAppMenuFacility<>(this);
        return Facility.enterSync(menu, () -> MENU_BUTTON.perform(click()));
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
                        .withIsOpeningTabs(0)
                        .withTabAlreadySelected(getLoadedTab())
                        .build();

        return loadPageProgramatically(destination, url);
    }

    /** Loads a |url| in the same tab and waits to transition to the given |destination|. */
    public <T extends PageStation> T loadPageProgramatically(T destination, String url) {
        Runnable r =
                () -> {
                    @PageTransition
                    int transitionType = PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR;
                    getActivity().getActivityTab().loadUrl(new LoadUrlParams(url, transitionType));
                };
        return Trip.travelSync(
                this,
                destination,
                Transition.timeoutOption(10000),
                () -> ThreadUtils.runOnUiThread(r));
    }

    /**
     * Returns the {@link ChromeTabbedActivity} matched to the ActivityCondition.
     *
     * <p>The element is only guaranteed to exist as long as the station is ACTIVE or in transition
     * triggers when it is already TRANSITIONING_FROM.
     */
    public ChromeTabbedActivity getActivity() {
        assertSuppliersCanBeUsed();
        return mActivityElement.get();
    }

    public Tab getLoadedTab() {
        assertSuppliersCanBeUsed();
        return mPageLoadedCondition.get();
    }

    private void assertSuppliersCanBeUsed() {
        int phase = getPhase();
        if (phase != Phase.ACTIVE && phase != Phase.TRANSITIONING_FROM) {
            fail(
                    String.format(
                            "%s should have been ACTIVE or TRANSITIONING_FROM, but was %s",
                            this, phaseToString(phase)));
        }
    }

    private class TabAddedCondition extends CallbackCondition implements TabModelObserver {
        private TabModel mTabModel;

        protected TabAddedCondition(int numTabsBeingOpened) {
            super("didAddTab", numTabsBeingOpened);
        }

        @Override
        public void didAddTab(Tab tab, int type, int creationState, boolean markedForSelection) {
            notifyCalled();
        }

        @Override
        public void onStartMonitoring() {
            super.onStartMonitoring();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabModel =
                                getTestRule()
                                        .getActivity()
                                        .getTabModelSelector()
                                        .getModel(isIncognito());
                        mTabModel.addObserver(this);
                    });
        }

        @Override
        public void onStopMonitoring() {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabModel.removeObserver(this);
                    });
        }
    }

    private class TabSelectedCondition extends CallbackCondition
            implements TabModelObserver, Supplier<Tab> {
        private final List<Tab> mTabsSelected = new ArrayList<>();
        private TabModel mTabModel;

        private TabSelectedCondition(int numTabsBeingSelected) {
            super("didSelectTab", numTabsBeingSelected);
        }

        @Override
        public void didSelectTab(Tab tab, int type, int lastId) {
            if (mTabsSelected.contains(tab)) {
                // We get multiple (2-3 depending on the case) didSelectTab when selecting a Tab, so
                // filter out redundant callbacks to make sure we wait for different Tabs.
                return;
            }
            mTabsSelected.add(tab);
            notifyCalled();
        }

        @Override
        public void onStartMonitoring() {
            super.onStartMonitoring();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabModel =
                                getTestRule()
                                        .getActivity()
                                        .getTabModelSelector()
                                        .getModel(isIncognito());
                        mTabModel.addObserver(this);
                    });
        }

        @Override
        public void onStopMonitoring() {
            super.onStopMonitoring();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabModel.removeObserver(this);
                    });
        }

        @Override
        public Tab get() {
            if (mTabsSelected.isEmpty()) {
                return null;
            }
            return mTabsSelected.get(mTabsSelected.size() - 1);
        }

        @Override
        public boolean hasValue() {
            return !mTabsSelected.isEmpty();
        }
    }

    private static class CorrectActivityTabCondition extends InstrumentationThreadCondition
            implements Supplier<Tab> {

        private final Supplier<ChromeTabbedActivity> mActivitySupplier;
        private final Supplier<Tab> mExpectedTab;
        private Tab mActivityTabMatched;

        private CorrectActivityTabCondition(
                Supplier<ChromeTabbedActivity> activitySupplier, Supplier<Tab> expectedTab) {
            super();
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeTabbedActivity");
            mExpectedTab = dependOnSupplier(expectedTab, "ExpectedTab");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            Tab currentActivityTab = mActivitySupplier.get().getActivityTab();
            if (currentActivityTab == null) {
                return notFulfilled("null activityTab");
            }

            Tab expectedTab = mExpectedTab.get();
            if (currentActivityTab == expectedTab) {
                mActivityTabMatched = currentActivityTab;
                return fulfilled("matched expected activityTab: " + mActivityTabMatched);
            } else {
                return notFulfilled(
                        "activityTab is " + currentActivityTab + ", expected " + expectedTab);
            }
        }

        @Override
        public String buildDescription() {
            return "Activity tab is the expected one";
        }

        @Override
        public Tab get() {
            return mActivityTabMatched;
        }

        @Override
        public boolean hasValue() {
            return mActivityTabMatched != null;
        }
    }

    private static class AnyActivityTabCondition extends InstrumentationThreadCondition
            implements Supplier<Tab> {

        private final Supplier<ChromeTabbedActivity> mActivitySupplier;
        private Tab mActivityTabMatched;

        private AnyActivityTabCondition(Supplier<ChromeTabbedActivity> activitySupplier) {
            super();
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeTabbedActivity");
        }

        @Override
        public ConditionStatus checkWithSuppliers() {
            Tab currentActivityTab = mActivitySupplier.get().getActivityTab();
            if (currentActivityTab == null) {
                return notFulfilled("null activityTab");
            } else {
                mActivityTabMatched = currentActivityTab;
                return fulfilled("found activityTab " + mActivityTabMatched);
            }
        }

        @Override
        public String buildDescription() {
            return "Activity has an activityTab";
        }

        @Override
        public Tab get() {
            return mActivityTabMatched;
        }

        @Override
        public boolean hasValue() {
            return mActivityTabMatched != null;
        }
    }
}
