// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import static org.chromium.base.test.transit.Condition.whether;

import com.google.errorprone.annotations.CheckReturnValue;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.transit.ChromeActivityTabModelBoundStation;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;
import java.util.function.Supplier;

/**
 * Base class for the screen that shows a web or native page in ChromeActivity. {@link
 * CtaPageStation} subclasses this for ChromeTabbedActivity and {@link CctPageStation} for
 * CustomTabActivity.
 *
 * <p>Contains extra configurable Conditions such as waiting for a tab to be created, selected, have
 * the expected title, etc.
 *
 * @param <HostActivity> The type of activity this station is associate to.
 */
public class BasePageStation<HostActivity extends ChromeActivity>
        extends ChromeActivityTabModelBoundStation<HostActivity> {

    /** Configuration for all BasePageStation subclasses. */
    public static class Config {
        protected boolean mIsEntryPoint;
        protected Boolean mIncognito;
        protected Integer mNumTabsBeingOpened;
        protected Integer mNumTabsBeingSelected;
        protected Tab mTabAlreadySelected;
        protected String mExpectedUrlSubstring;
        protected String mExpectedTitle;

        public Config withIncognito(boolean incognito) {
            mIncognito = incognito;
            return this;
        }

        public Config withIsOpeningTabs(int numTabsBeingOpened) {
            assert numTabsBeingOpened >= 0;
            mNumTabsBeingOpened = numTabsBeingOpened;
            return this;
        }

        public Config withTabAlreadySelected(Tab currentTab) {
            mTabAlreadySelected = currentTab;
            mNumTabsBeingSelected = 0;
            return this;
        }

        public Config withIsSelectingTabs(int numTabsBeingSelected) {
            assert numTabsBeingSelected > 0
                    : "Use withIsSelectingTab() if the PageStation is still in the current tab";
            mNumTabsBeingSelected = numTabsBeingSelected;
            // Commonly already set via initFrom().
            mTabAlreadySelected = null;
            return this;
        }

        public Config withEntryPoint() {
            mNumTabsBeingOpened = 0;
            mNumTabsBeingSelected = 0;
            mIsEntryPoint = true;
            return this;
        }

        public Config withExpectedUrlSubstring(String value) {
            mExpectedUrlSubstring = value;
            return this;
        }

        public Config withExpectedTitle(String title) {
            mExpectedTitle = title;
            return this;
        }

        public Config initFrom(BasePageStation<?> previousStation) {
            if (mIncognito == null) {
                mIncognito = previousStation.mIsIncognito;
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
            return this;
        }
    }

    /**
     * Basic builder for all BasePageStation subclasses.
     *
     * @param <PageT> the subclass of BasePageStation to build.
     */
    public static class Builder<PageT extends BasePageStation<? extends ChromeActivity>> {
        protected final Function<Config, PageT> mFactoryMethod;
        protected final Config mConfig;

        public Builder(Function<Config, PageT> factoryMethod) {
            mFactoryMethod = factoryMethod;
            mConfig = new Config();
        }

        public Builder<PageT> withIncognito(boolean incognito) {
            mConfig.withIncognito(incognito);
            return this;
        }

        public Builder<PageT> withIsOpeningTabs(int numTabsBeingOpened) {
            mConfig.withIsOpeningTabs(numTabsBeingOpened);
            return this;
        }

        /** Do not wait for a tba to be selected; use the current tab. */
        public Builder<PageT> withTabAlreadySelected(Tab currentTab) {
            mConfig.withIsOpeningTabs(0);
            mConfig.withTabAlreadySelected(currentTab);
            return this;
        }

        public Builder<PageT> withIsSelectingTabs(int numTabsBeingSelected) {
            mConfig.withIsSelectingTabs(numTabsBeingSelected);
            return this;
        }

        public Builder<PageT> withEntryPoint() {
            mConfig.withEntryPoint();
            return this;
        }

        public Builder<PageT> withExpectedUrlSubstring(String value) {
            mConfig.withExpectedUrlSubstring(value);
            return this;
        }

        public Builder<PageT> withExpectedTitle(String title) {
            mConfig.withExpectedTitle(title);
            return this;
        }

        public Builder<PageT> initFrom(BasePageStation<?> previousStation) {
            mConfig.initFrom(previousStation);
            return this;
        }

        /** Wait for the |url| to be loaded on the current tab. */
        public Builder<PageT> initForLoadingUrlOnSameTab(
                String url, BasePageStation<?> previousStation) {
            initFrom(previousStation);
            if (mConfig.mExpectedUrlSubstring == null) {
                mConfig.withExpectedUrlSubstring(url);
            }
            return this;
        }

        /** Wait for a new tab to be opened and selected. */
        public Builder<PageT> initOpeningNewTab() {
            mConfig.withIsOpeningTabs(1);
            mConfig.withIsSelectingTabs(1);
            return this;
        }

        /** Wait for an existing tab to be selected. */
        public Builder<PageT> initSelectingExistingTab() {
            mConfig.withIsOpeningTabs(0);
            mConfig.withIsSelectingTabs(1);
            return this;
        }

        public PageT build() {
            return mFactoryMethod.apply(mConfig);
        }
    }

    public final Element<Tab> activityTabElement;
    public final Element<Tab> loadedTabElement;

    protected BasePageStation(Class<HostActivity> activityClass, Config config) {
        // incognito is optional and defaults to false
        super(activityClass, config.mIncognito == null ? false : config.mIncognito);

        // mNumTabsBeingOpened is required
        assert config.mNumTabsBeingOpened != null
                : "PageStation.Builder needs withIsOpeningTabs() or initFrom()";

        // mNumTabsBeingSelected is required
        assert config.mNumTabsBeingSelected != null
                : "PageStation.Builder needs withIsSelectingTabs(), withTabAlreadySelected() or"
                        + " initFrom()";

        // Pages must have an already selected tab, or be selecting a tab.
        assert config.mIsEntryPoint
                        || (config.mTabAlreadySelected != null)
                                != (config.mNumTabsBeingSelected != 0)
                : String.format(
                        "mTabAlreadySelected=%s mNumTabsBeingSelected=%s",
                        config.mTabAlreadySelected, config.mNumTabsBeingSelected);

        if (config.mNumTabsBeingOpened > 0) {
            declareEnterCondition(
                    new TabAddedCondition(config.mNumTabsBeingOpened, tabModelElement));
        }

        // isEntryPoint is optional and defaults to false
        if (config.mIsEntryPoint) {
            // In entry points we just match the first ActivityTab we see, instead of waiting for
            // callbacks.
            activityTabElement =
                    declareEnterConditionAsElement(new AnyActivityTabCondition<>(mActivityElement));
        } else {
            Supplier<Tab> mSelectedTabSupplier;
            if (config.mNumTabsBeingSelected > 0) {
                // The last tab of N opened is the Tab that mSelectedTabSupplier will supply.
                TabSelectedCondition tabSelectedCondition =
                        new TabSelectedCondition(config.mNumTabsBeingSelected, tabModelElement);
                declareEnterCondition(tabSelectedCondition);
                mSelectedTabSupplier = tabSelectedCondition;
            } else {
                // The Tab already created and provided to the constructor is the one that is
                // expected to be the activityTab.
                mSelectedTabSupplier = () -> config.mTabAlreadySelected;
            }
            // Only returns the tab when it is the activityTab.
            activityTabElement =
                    declareEnterConditionAsElement(
                            new CorrectActivityTabCondition<>(
                                    mActivityElement, mSelectedTabSupplier));
        }
        loadedTabElement =
                declareEnterConditionAsElement(
                        new PageLoadedCondition(activityTabElement, mIsIncognito));

        declareEnterCondition(new PageInteractableOrHiddenCondition(loadedTabElement));

        // URL substring is optional.
        if (config.mExpectedUrlSubstring != null) {
            declareEnterCondition(
                    new PageUrlContainsCondition(config.mExpectedUrlSubstring, loadedTabElement));
        }

        // title is optional
        if (config.mExpectedTitle != null) {
            declareEnterCondition(new PageTitleCondition(config.mExpectedTitle, loadedTabElement));
        }
    }

    /** Convenience method for |loadedTabElement.get()|. */
    public Tab getTab() {
        return loadedTabElement.value();
    }

    /** Loads a |url| in the same tab and waits to transition to the Station built by |builder|. */
    public <DestinationT extends BasePageStation<HostActivity>>
            DestinationT loadPageProgrammatically(String url, Builder<DestinationT> builder) {
        return loadUrlTo(url).arriveAt(builder.initForLoadingUrlOnSameTab(url, this).build());
    }

    /** Loads a |url| in the same tab to start a Trip. */
    @CheckReturnValue
    public TripBuilder loadUrlTo(String url) {
        return runOnUiThreadTo(
                        () -> {
                            @PageTransition
                            int transitionType =
                                    PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR;
                            loadedTabElement
                                    .value()
                                    .loadUrl(new LoadUrlParams(url, transitionType));
                        })
                .withTimeout(10000)
                .withPossiblyAlreadyFulfilled()
                .waitForAnd(new PageLoadCallbackCondition(loadedTabElement.value()));
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

    private static class TabAddedCondition extends CallbackCondition implements TabModelObserver {
        private TabModel mTabModel;
        private final Supplier<TabModel> mTabModelSupplier;

        protected TabAddedCondition(int numTabsBeingOpened, Supplier<TabModel> tabModelSupplier) {
            super("didAddTab", numTabsBeingOpened);
            mTabModelSupplier = dependOnSupplier(tabModelSupplier, "TabModel");
        }

        @Override
        public void didAddTab(
                Tab tab,
                @TabLaunchType int type,
                @TabCreationState int creationState,
                boolean markedForSelection) {
            notifyCalled();
        }

        @Override
        public void onStartMonitoring() {
            super.onStartMonitoring();
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mTabModel = mTabModelSupplier.get();
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

    private static class TabSelectedCondition extends CallbackCondition
            implements TabModelObserver, Supplier<Tab> {
        private final List<Tab> mTabsSelected = new ArrayList<>();
        private final Supplier<TabModel> mTabModelSupplier;
        private TabModel mTabModel;

        private TabSelectedCondition(
                int numTabsBeingSelected, Supplier<TabModel> tabModelSupplier) {
            super("didSelectTab", numTabsBeingSelected);
            mTabModelSupplier = dependOnSupplier(tabModelSupplier, "ChromeActivity");
        }

        @Override
        public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
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
                        mTabModel = mTabModelSupplier.get();
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
    }

    private static class CorrectActivityTabCondition<ActivityT extends ChromeActivity>
            extends ConditionWithResult<Tab> {

        private final Supplier<ActivityT> mActivitySupplier;
        private final Supplier<Tab> mExpectedTab;

        private CorrectActivityTabCondition(
                Supplier<ActivityT> activitySupplier, Supplier<Tab> expectedTabSupplier) {
            super(/* isRunOnUiThread= */ true);
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
            super(/* isRunOnUiThread= */ true);
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
