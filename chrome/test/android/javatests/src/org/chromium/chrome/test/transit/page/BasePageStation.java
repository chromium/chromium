// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static org.chromium.base.test.transit.Condition.whether;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.Transition.Trigger;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;

/**
 * Base class for the screen that shows a web or native page in ChromeActivity. {@link PageStation}
 * subclasses this for ChromeTabbedActivity and {@link CctPageStation} for CustomTabActivity.
 *
 * <p>Contains extra configurable Conditions such as waiting for a tab to be created, selected, have
 * the expected title, etc.
 *
 * @param <HostActivity> The type of activity this station is associate to.
 */
public class BasePageStation<HostActivity extends ChromeActivity> extends Station<HostActivity> {

    /**
     * Basic builder for all BasePageStation subclasses.
     *
     * @param <ActivityT> the subclass of ChromeActivity on which the BasePageStation being built is
     *     based upon.
     * @param <PageT> the subclass of BasePageStation to build.
     * @param <BuilderT> the subclass of this Builder.
     */
    public static class Builder<
            ActivityT extends ChromeActivity,
            PageT extends BasePageStation<ActivityT>,
            BuilderT extends Builder<ActivityT, PageT, BuilderT>> {
        protected final Function<BuilderT, PageT> mFactoryMethod;
        protected boolean mIsEntryPoint;
        protected Boolean mIncognito;
        protected Integer mNumTabsBeingOpened;
        protected Integer mNumTabsBeingSelected;
        protected Tab mTabAlreadySelected;
        protected String mExpectedUrlSubstring;
        protected String mExpectedTitle;
        protected List<Facility<PageT>> mFacilities;

        public Builder(Function<BuilderT, PageT> factoryMethod) {
            mFactoryMethod = factoryMethod;
        }

        public BuilderT self() {
            return (BuilderT) this;
        }

        public BuilderT withIncognito(boolean incognito) {
            mIncognito = incognito;
            return self();
        }

        public BuilderT withIsOpeningTabs(int numTabsBeingOpened) {
            assert numTabsBeingOpened >= 0;
            mNumTabsBeingOpened = numTabsBeingOpened;
            return self();
        }

        public BuilderT withTabAlreadySelected(Tab currentTab) {
            mTabAlreadySelected = currentTab;
            mNumTabsBeingSelected = 0;
            return self();
        }

        public BuilderT withIsSelectingTabs(int numTabsBeingSelected) {
            assert numTabsBeingSelected > 0
                    : "Use withIsSelectingTab() if the PageStation is still in the current tab";
            mNumTabsBeingSelected = numTabsBeingSelected;
            // Commonly already set via initFrom().
            mTabAlreadySelected = null;
            return self();
        }

        public BuilderT withEntryPoint() {
            mNumTabsBeingOpened = 0;
            mNumTabsBeingSelected = 0;
            mIsEntryPoint = true;
            return self();
        }

        public BuilderT withExpectedUrlSubstring(String value) {
            mExpectedUrlSubstring = value;
            return self();
        }

        public BuilderT withExpectedTitle(String title) {
            mExpectedTitle = title;
            return self();
        }

        public BuilderT withFacility(Facility<PageT> facility) {
            if (mFacilities == null) {
                mFacilities = new ArrayList<>();
            }
            mFacilities.add(facility);
            return self();
        }

        public BuilderT initFrom(BasePageStation<ActivityT> previousStation) {
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
                mTabAlreadySelected = previousStation.loadedTabElement.getFromPast();
            }
            // Cannot copy over facilities because we have no way to clone them. It's also not
            // obvious that we should...
            return self();
        }

        public PageT build() {
            return mFactoryMethod.apply(self());
        }
    }

    protected final boolean mIncognito;
    public Element<Tab> activityTabElement;
    public Element<Tab> loadedTabElement;

    protected <T extends BasePageStation<HostActivity>> BasePageStation(
            Class<HostActivity> activityClass, Builder<HostActivity, T, ?> builder) {
        super(activityClass);

        // incognito is optional and defaults to false
        mIncognito = builder.mIncognito == null ? false : builder.mIncognito;

        // mNumTabsBeingOpened is required
        assert builder.mNumTabsBeingOpened != null
                : "PageStation.Builder needs withIsOpeningTabs() or initFrom()";

        // mNumTabsBeingSelected is required
        assert builder.mNumTabsBeingSelected != null
                : "PageStation.Builder needs withIsSelectingTabs(), withTabAlreadySelected() or"
                        + " initFrom()";

        // Pages must have an already selected tab, or be selecting a tab.
        assert builder.mIsEntryPoint
                        || (builder.mTabAlreadySelected != null)
                                != (builder.mNumTabsBeingSelected != 0)
                : String.format(
                        "mTabAlreadySelected=%s mNumTabsBeingSelected=%s",
                        builder.mTabAlreadySelected, builder.mNumTabsBeingSelected);

        if (builder.mFacilities != null) {
            for (Facility<T> facility : builder.mFacilities) {
                addInitialFacility(facility);
            }
        }

        if (builder.mNumTabsBeingOpened > 0) {
            declareEnterCondition(
                    new TabAddedCondition<>(builder.mNumTabsBeingOpened, mActivityElement));
        }

        // isEntryPoint is optional and defaults to false
        if (builder.mIsEntryPoint) {
            // In entry points we just match the first ActivityTab we see, instead of waiting for
            // callbacks.
            activityTabElement =
                    declareEnterConditionAsElement(new AnyActivityTabCondition<>(mActivityElement));
        } else {
            Supplier<Tab> mSelectedTabSupplier;
            if (builder.mNumTabsBeingSelected > 0) {
                // The last tab of N opened is the Tab that mSelectedTabSupplier will supply.
                TabSelectedCondition<HostActivity> tabSelectedCondition =
                        new TabSelectedCondition<>(builder.mNumTabsBeingSelected, mActivityElement);
                declareEnterCondition(tabSelectedCondition);
                mSelectedTabSupplier = tabSelectedCondition;
            } else {
                // The Tab already created and provided to the constructor is the one that is
                // expected to be the activityTab.
                mSelectedTabSupplier = () -> builder.mTabAlreadySelected;
            }
            // Only returns the tab when it is the activityTab.
            activityTabElement =
                    declareEnterConditionAsElement(
                            new CorrectActivityTabCondition<>(
                                    mActivityElement, mSelectedTabSupplier));
        }
        loadedTabElement =
                declareEnterConditionAsElement(
                        new PageLoadedCondition(activityTabElement, mIncognito));

        declareEnterCondition(new PageInteractableOrHiddenCondition(loadedTabElement));

        // URL substring is optional.
        if (builder.mExpectedUrlSubstring != null) {
            declareEnterCondition(
                    new PageUrlContainsCondition(builder.mExpectedUrlSubstring, loadedTabElement));
        }

        // title is optional
        if (builder.mExpectedTitle != null) {
            declareEnterCondition(new PageTitleCondition(builder.mExpectedTitle, loadedTabElement));
        }
    }

    public boolean isIncognito() {
        return mIncognito;
    }

    /** Loads a |url| in the same tab and waits to transition. */
    public <DestinationT extends BasePageStation<HostActivity>>
            DestinationT loadPageProgrammatically(
                    String url, Builder<HostActivity, DestinationT, ?> builder) {
        builder.initFrom(this);
        if (builder.mExpectedUrlSubstring == null) {
            builder.mExpectedUrlSubstring = url;
        }

        DestinationT destination = builder.build();
        Trigger trigger =
                () -> {
                    @PageTransition
                    int transitionType = PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR;
                    loadedTabElement.get().loadUrl(new LoadUrlParams(url, transitionType));
                };
        Transition.TransitionOptions options =
                Transition.newOptions()
                        .withCondition(new PageLoadCallbackCondition(loadedTabElement.get()))
                        .withTimeout(10000)
                        .withPossiblyAlreadyFulfilled()
                        .withRunTriggerOnUiThread()
                        .build();
        return travelToSync(destination, options, trigger);
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

    private class TabAddedCondition<ActivityT extends ChromeActivity> extends CallbackCondition
            implements TabModelObserver {
        private TabModel mTabModel;
        private final Supplier<ActivityT> mActivitySupplier;

        protected TabAddedCondition(int numTabsBeingOpened, Supplier<ActivityT> activitySupplier) {
            super("didAddTab", numTabsBeingOpened);
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeActivity");
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
            super.onStopMonitoring();
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabModel.removeObserver(this);
                    });
        }
    }

    private class TabSelectedCondition<ActivityT extends ChromeActivity> extends CallbackCondition
            implements TabModelObserver, Supplier<Tab> {
        private final List<Tab> mTabsSelected = new ArrayList<>();
        private TabModel mTabModel;
        private final Supplier<ActivityT> mActivitySupplier;

        private TabSelectedCondition(
                int numTabsBeingSelected, Supplier<ActivityT> activitySupplier) {
            super("didSelectTab", numTabsBeingSelected);
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeActivity");
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

    private static class CorrectActivityTabCondition<ActivityT extends ChromeActivity>
            extends ConditionWithResult<Tab> {

        private final Supplier<ActivityT> mActivitySupplier;
        private final Supplier<Tab> mExpectedTab;

        private CorrectActivityTabCondition(
                Supplier<ActivityT> activitySupplier, Supplier<Tab> expectedTabSupplier) {
            super(/* isRunOnUiThread= */ false);
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeActivity");
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

    private static class AnyActivityTabCondition<ActivityT extends ChromeActivity>
            extends ConditionWithResult<Tab> {

        private final Supplier<ActivityT> mActivitySupplier;

        private AnyActivityTabCondition(Supplier<ActivityT> activitySupplier) {
            super(/* isRunOnUiThread= */ false);
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeActivity");
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
