// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ActivityElement;
import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.Elements;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.content_public.browser.LoadUrlParams;
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
        private boolean mIsEntryPoint;
        private Boolean mIncognito;
        private Integer mNumTabsBeingOpened;
        private Integer mNumTabsBeingSelected;
        private Tab mTabAlreadySelected;
        private String mExpectedUrlSubstring;
        private String mExpectedTitle;
        private List<Facility<T>> mFacilities;

        public Builder(Function<Builder<T>, T> factoryMethod) {
            mFactoryMethod = factoryMethod;
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
            // Commonly already set via initFrom().
            mTabAlreadySelected = null;
            return this;
        }

        public Builder<T> withEntryPoint() {
            mNumTabsBeingOpened = 0;
            mNumTabsBeingSelected = 0;
            mIsEntryPoint = true;
            return this;
        }

        public Builder<T> withExpectedUrlSubstring(String value) {
            mExpectedUrlSubstring = value;
            return this;
        }

        public Builder<T> withExpectedTitle(String title) {
            mExpectedTitle = title;
            return this;
        }

        public Builder<T> withFacility(Facility<T> facility) {
            if (mFacilities == null) {
                mFacilities = new ArrayList<>();
            }
            mFacilities.add(facility);
            return this;
        }

        public Builder<T> initFrom(PageStation previousStation) {
            if (mIncognito == null) {
                mIncognito = previousStation.mIncognito;
            }
            if (mNumTabsBeingOpened == null) {
                mNumTabsBeingOpened = 0;
            }
            if (mNumTabsBeingSelected == null) {
                mNumTabsBeingSelected = 0;
            }
            if (mTabAlreadySelected == null && mNumTabsBeingSelected == 0) {
                mTabAlreadySelected = previousStation.getLoadedTab();
            }
            // Cannot copy over facilities because we have no way to clone them. It's also not
            // obvious that we should...
            return this;
        }

        public T build() {
            return mFactoryMethod.apply(this);
        }
    }

    protected final boolean mIncognito;
    protected final boolean mIsEntryPoint;
    protected final int mNumTabsBeingOpened;
    protected final int mNumTabsBeingSelected;
    protected final Tab mTabAlreadySelected;
    protected final String mExpectedUrlSubstring;
    protected final String mExpectedTitle;

    public static final ViewSpec HOME_BUTTON = viewSpec(withId(R.id.home_button));
    public static final ViewSpec URL_BAR = viewSpec(withId(R.id.url_bar));
    public static final ViewSpec TAB_SWITCHER_BUTTON = viewSpec(withId(R.id.tab_switcher_button));
    public static final ViewSpec MENU_BUTTON = viewSpec(withId(R.id.menu_button));

    protected ActivityElement<ChromeTabbedActivity> mActivityElement;
    protected Supplier<Tab> mActivityTabSupplier;
    protected Supplier<Tab> mSelectedTabSupplier;
    protected Supplier<Tab> mPageLoadedSupplier;

    /** Use the PageStation's subclass |newBuilder()|. */
    protected <T extends PageStation> PageStation(Builder<T> builder) {
        // incognito is optional and defaults to false
        mIncognito = builder.mIncognito == null ? false : builder.mIncognito;

        // isEntryPoint is optional and defaults to false
        mIsEntryPoint = builder.mIsEntryPoint;

        // isOpeningTab is required
        assert builder.mNumTabsBeingOpened != null;
        mNumTabsBeingOpened = builder.mNumTabsBeingOpened;

        // mNumTabsBeingSelected is required
        assert builder.mNumTabsBeingSelected != null;
        mNumTabsBeingSelected = builder.mNumTabsBeingSelected;

        // Pages must have an already selected tab, or be selecting a tab.
        mTabAlreadySelected = builder.mTabAlreadySelected;
        assert mIsEntryPoint || (mTabAlreadySelected != null) != (mNumTabsBeingSelected != 0)
                : String.format(
                        "mTabAlreadySelected=%s mNumTabsBeingSelected=%s",
                        mTabAlreadySelected, mNumTabsBeingSelected);

        // URL substring is optional.
        mExpectedUrlSubstring = builder.mExpectedUrlSubstring;

        // title is optional
        mExpectedTitle = builder.mExpectedTitle;

        if (builder.mFacilities != null) {
            for (Facility<T> facility : builder.mFacilities) {
                addInitialFacility(facility);
            }
        }
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        mActivityElement = elements.declareActivity(ChromeTabbedActivity.class);

        // TODO(crbug.com/41497463): These should be scoped, but for now they need to be unscoped
        // since they unintentionally still exist in the non-Hub tab switcher. They are mostly
        // occluded by the tab switcher toolbar, but at least the tab_switcher_button is still
        // visible.
        elements.declareView(HOME_BUTTON, ViewElement.unscopedOption());
        elements.declareView(TAB_SWITCHER_BUTTON, ViewElement.unscopedOption());
        elements.declareView(MENU_BUTTON, ViewElement.unscopedOption());

        if (mNumTabsBeingOpened > 0) {
            elements.declareEnterCondition(
                    new TabAddedCondition(mNumTabsBeingOpened, mActivityElement));
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
                                new TabSelectedCondition(mNumTabsBeingSelected, mActivityElement));
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
        mPageLoadedSupplier =
                elements.declareEnterCondition(
                        new PageLoadedCondition(mActivityTabSupplier, mIncognito));

        elements.declareEnterCondition(new PageInteractableOrHiddenCondition(mPageLoadedSupplier));

        if (mExpectedTitle != null) {
            elements.declareEnterCondition(
                    new PageTitleCondition(mExpectedTitle, mPageLoadedSupplier));
        }
        if (mExpectedUrlSubstring != null) {
            elements.declareEnterCondition(
                    new PageUrlContainsCondition(mExpectedUrlSubstring, mPageLoadedSupplier));
        }
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
        return enterFacilitySync(
                new TabSwitcherActionMenuFacility(),
                () -> TAB_SWITCHER_BUTTON.perform(longClick()));
    }

    /** Opens the app menu by pressing the toolbar "..." button */
    public PageAppMenuFacility<PageStation> openGenericAppMenu() {
        recheckActiveConditions();

        return enterFacilitySync(new PageAppMenuFacility<PageStation>(), MENU_BUTTON::click);
    }

    /** Opens the tab switcher by pressing the toolbar tab switcher button. */
    public RegularTabSwitcherStation openRegularTabSwitcher() {
        assert !mIncognito;
        return travelToSync(
                RegularTabSwitcherStation.from(getActivity().getTabModelSelector()),
                TAB_SWITCHER_BUTTON::click);
    }

    /** Opens the incognito tab switcher by pressing the toolbar tab switcher button. */
    public IncognitoTabSwitcherStation openIncognitoTabSwitcher() {
        assert mIncognito;
        return travelToSync(
                IncognitoTabSwitcherStation.from(getActivity().getTabModelSelector()),
                TAB_SWITCHER_BUTTON::click);
    }

    /** Loads a |url| in the same tab and waits to transition. */
    public <T extends PageStation> T loadPageProgrammatically(String url, Builder<T> builder) {
        builder.initFrom(this);
        if (builder.mExpectedUrlSubstring == null) {
            builder.mExpectedUrlSubstring = url;
        }

        T destination = builder.build();
        Runnable r =
                () -> {
                    @PageTransition
                    int transitionType = PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR;
                    getActivity().getActivityTab().loadUrl(new LoadUrlParams(url, transitionType));
                };
        // TODO(b/341978208): Wait for a page loaded callback.
        Transition.TransitionOptions options =
                Transition.newOptions().withTimeout(10000).withPossiblyAlreadyFulfilled().build();
        return travelToSync(destination, options, () -> ThreadUtils.runOnUiThread(r));
    }

    public WebPageStation loadAboutBlank() {
        return loadPageProgrammatically("about:blank", WebPageStation.newBuilder());
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
        return mPageLoadedSupplier.get();
    }

    private class TabAddedCondition extends CallbackCondition implements TabModelObserver {
        private TabModel mTabModel;
        private Supplier<ChromeTabbedActivity> mActivitySupplier;

        protected TabAddedCondition(
                int numTabsBeingOpened, Supplier<ChromeTabbedActivity> activitySupplier) {
            super("didAddTab", numTabsBeingOpened);
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeTabbedActivity");
        }

        @Override
        public void didAddTab(Tab tab, int type, int creationState, boolean markedForSelection) {
            notifyCalled();
        }

        @Override
        public void onStartMonitoring() {
            super.onStartMonitoring();
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabModel =
                                mActivitySupplier
                                        .get()
                                        .getTabModelSelector()
                                        .getModel(isIncognito());
                        mTabModel.addObserver(this);
                    });
        }

        @Override
        public void onStopMonitoring() {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabModel.removeObserver(this);
                    });
        }
    }

    private class TabSelectedCondition extends CallbackCondition
            implements TabModelObserver, Supplier<Tab> {
        private final List<Tab> mTabsSelected = new ArrayList<>();
        private TabModel mTabModel;
        private Supplier<ChromeTabbedActivity> mActivitySupplier;

        private TabSelectedCondition(
                int numTabsBeingSelected, Supplier<ChromeTabbedActivity> activitySupplier) {
            super("didSelectTab", numTabsBeingSelected);
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeTabbedActivity");
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
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabModel =
                                mActivitySupplier
                                        .get()
                                        .getTabModelSelector()
                                        .getModel(isIncognito());
                        mTabModel.addObserver(this);
                    });
        }

        @Override
        public void onStopMonitoring() {
            super.onStopMonitoring();
            ThreadUtils.runOnUiThreadBlocking(
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

    private static class CorrectActivityTabCondition extends ConditionWithResult<Tab> {

        private final Supplier<ChromeTabbedActivity> mActivitySupplier;
        private final Supplier<Tab> mExpectedTab;

        private CorrectActivityTabCondition(
                Supplier<ChromeTabbedActivity> activitySupplier,
                Supplier<Tab> expectedTabSupplier) {
            super(/* isRunOnUiThread= */ false);
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeTabbedActivity");
            mExpectedTab = dependOnSupplier(expectedTabSupplier, "ExpectedTab");
        }

        @Override
        protected ConditionStatusWithResult<Tab> resolveWithSuppliers() {
            Tab currentActivityTab = mActivitySupplier.get().getActivityTab();
            if (currentActivityTab == null) {
                return notFulfilled("null activityTab").withoutResult();
            }

            Tab expectedTab = mExpectedTab.get();
            if (currentActivityTab == expectedTab) {
                return fulfilled("matched expected activityTab: " + currentActivityTab)
                        .withResult(currentActivityTab);
            } else {
                return notFulfilled(
                                "activityTab is "
                                        + currentActivityTab
                                        + ", expected "
                                        + expectedTab)
                        .withoutResult();
            }
        }

        @Override
        public String buildDescription() {
            return "Activity tab is the expected one";
        }
    }

    private static class AnyActivityTabCondition extends ConditionWithResult<Tab> {

        private final Supplier<ChromeTabbedActivity> mActivitySupplier;

        private AnyActivityTabCondition(Supplier<ChromeTabbedActivity> activitySupplier) {
            super(/* isRunOnUiThread= */ false);
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeTabbedActivity");
        }

        @Override
        protected ConditionStatusWithResult<Tab> resolveWithSuppliers() {
            Tab currentActivityTab = mActivitySupplier.get().getActivityTab();
            if (currentActivityTab == null) {
                return notFulfilled("null activityTab").withoutResult();
            } else {
                return fulfilled("found activityTab " + currentActivityTab)
                        .withResult(currentActivityTab);
            }
        }

        @Override
        public String buildDescription() {
            return "Activity has an activityTab";
        }
    }
}
