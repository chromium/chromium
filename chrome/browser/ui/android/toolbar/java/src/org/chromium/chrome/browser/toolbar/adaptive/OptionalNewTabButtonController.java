// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorUtil;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.top.tab_strip.StripVisibilityState;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.function.Supplier;

/**
 * Optional toolbar button which opens a new tab. May be used by {@link
 * AdaptiveToolbarButtonController}.
 */
@NullMarked
public class OptionalNewTabButtonController extends BaseButtonDataProvider
        implements ConfigurationChangedObserver {
    /**
     * Set of methods used to interact with dependencies which may require native libraries to
     * function. Robolectric tests can use shadows to inject dependencies in tests.
     */
    @VisibleForTesting
    /* package */ static class Delegate {
        private final Supplier<@Nullable TabCreatorManager> mTabCreatorManagerSupplier;
        private final Supplier<@Nullable Tab> mActiveTabSupplier;

        public Delegate(
                Supplier<@Nullable TabCreatorManager> tabCreatorManagerSupplier,
                Supplier<@Nullable Tab> activeTabSupplier) {
            mTabCreatorManagerSupplier = tabCreatorManagerSupplier;
            mActiveTabSupplier = activeTabSupplier;
        }

        /** Returns a {@link TabCreatorManager} used for creating the new tab. */
        @Nullable TabCreatorManager getTabCreatorManager() {
            return mTabCreatorManagerSupplier.get();
        }

        /**
         * Returns a {@link TabModelSelector} used for obtaining the current tab and the incognito
         * state.
         *
         * <p>Not using {@link IncognitoStateProvider} because ISP is created in the {@link
         * ToolbarManager} and not in {@link RootUiCoordinator}.
         *
         * <p>TODO(crbug.com/40753461): Make IncognitoStateProvider available in RootUiCooridnator.
         */
        Supplier<@Nullable Tab> getActiveTabSupplier() {
            return mActiveTabSupplier;
        }
    }

    /** Context used for fetching resources and window size. */
    private final Context mContext;

    private final Delegate mDelegate;
    private final Supplier<@Nullable Tracker> mTrackerSupplier;

    private boolean mIsTablet;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final ObservableSupplier<@StripVisibilityState Integer> mTabStripVisibilitySupplier;
    private final Callback<Integer> mOnTabStripVisibilityStateChanged =
            this::onTabStripVisibilityStateChanged;

    /**
     * Creates {@code OptionalNewTabButtonController}.
     *
     * @param context The Context for retrieving resources, etc.
     * @param buttonDrawable Drawable for the new tab button.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events, e.g.
     *     configuration changes.
     * @param tabCreatorManagerSupplier Used to open new tabs.
     * @param activeTabSupplier Used to access the current tab.
     * @param trackerSupplier Supplier for the current profile tracker.
     * @param tabStripVisibilitySupplier Supplier for the visibility of the tab strip.
     */
    public OptionalNewTabButtonController(
            Context context,
            Drawable buttonDrawable,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Supplier<@Nullable TabCreatorManager> tabCreatorManagerSupplier,
            Supplier<@Nullable Tab> activeTabSupplier,
            Supplier<@Nullable Tracker> trackerSupplier,
            ObservableSupplier<@StripVisibilityState Integer> tabStripVisibilitySupplier) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                buttonDrawable,
                context.getString(R.string.button_new_tab),
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.NEW_TAB,
                /* tooltipTextResId= */ R.string.new_tab_title);
        setShouldShowOnIncognitoTabs(true);

        mContext = context;
        mDelegate = new Delegate(tabCreatorManagerSupplier, activeTabSupplier);
        mTrackerSupplier = trackerSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);

        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        mTabStripVisibilitySupplier = tabStripVisibilitySupplier;
        if (ChromeFeatureList.sToolbarTabletResizeRefactor.isEnabled()) {
            mTabStripVisibilitySupplier.addObserver(mOnTabStripVisibilityStateChanged);
        }
    }

    @Override
    public void onClick(View view) {
        Supplier<@Nullable Tab> activeTabSupplier = mDelegate.getActiveTabSupplier();
        if (activeTabSupplier == null || activeTabSupplier.get() == null) return;

        TabCreatorManager tabCreatorManager = mDelegate.getTabCreatorManager();
        if (tabCreatorManager == null) return;

        boolean isIncognito = activeTabSupplier.get().isIncognito();
        RecordUserAction.record("MobileTopToolbarOptionalButtonNewTab");
        TabCreatorUtil.launchNtp(tabCreatorManager.getTabCreator(isIncognito));

        Tracker tracker = mTrackerSupplier.get();
        if (tracker != null) {
            tracker.notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_NEW_TAB_OPENED);
        }
    }

    private void onTabStripVisibilityStateChanged(@StripVisibilityState int tabStripVisibility) {
        mButtonData.setCanShow(shouldShowButton(mActiveTabSupplier.get()));
        notifyObservers(true);
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        if (mIsTablet == isTablet && !ChromeFeatureList.sToolbarTabletResizeRefactor.isEnabled()) {
            return;
        }
        mIsTablet = isTablet;

        mButtonData.setCanShow(shouldShowButton(mActiveTabSupplier.get()));
    }

    @Override
    protected boolean shouldShowButton(@Nullable Tab tab) {
        if (tab == null) return false;
        if (!super.shouldShowButton(tab)) return false;
        // On tablets, the new tab button can be shown when the tab strip is not visible, if the
        // tablet toolbar resize refactor is enabled.
        if (mIsTablet) {
            if (!ChromeFeatureList.sToolbarTabletResizeRefactor.isEnabled()
                    || mTabStripVisibilitySupplier.get() == null
                    || mTabStripVisibilitySupplier.get() != StripVisibilityState.HIDDEN_BY_FADE) {
                return false;
            }
        }
        if (UrlUtilities.isNtpUrl(tab.getUrl())) return false;

        return true;
    }

    /**
     * Returns an IPH for this button. Only called once native is initialized and when {@code
     * AdaptiveToolbarFeatures.isCustomizationEnabled()} is true.
     *
     * @param tab Current tab.
     */
    @Override
    protected IphCommandBuilder getIphCommandBuilder(Tab tab) {
        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
        IphCommandBuilder iphCommandBuilder =
                new IphCommandBuilder(
                                tab.getContext().getResources(),
                                FeatureConstants
                                        .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_NEW_TAB_FEATURE,
                                /* stringId= */ R.string.adaptive_toolbar_button_new_tab_iph,
                                /* accessibilityStringId= */ R.string
                                        .adaptive_toolbar_button_new_tab_iph)
                        .setHighlightParams(params);
        return iphCommandBuilder;
    }

    @Override
    public void destroy() {
        if (mTabStripVisibilitySupplier != null) {
            mTabStripVisibilitySupplier.removeObserver(mOnTabStripVisibilityStateChanged);
        }
        super.destroy();
    }

    void setIsTabletForTesting(boolean isTablet) {
        mIsTablet = isTablet;
    }
}
