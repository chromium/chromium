// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Optional toolbar button which opens a new tab. May be used by {@link
 * AdaptiveToolbarButtonController}.
 */
public class OptionalNewTabButtonController extends BaseButtonDataProvider
        implements ConfigurationChangedObserver {
    /**
     * Set of methods used to interact with dependencies which may require native libraries to
     * function. Robolectric tests can use shadows to inject dependencies in tests.
     */
    @VisibleForTesting
    /* package */ static class Delegate {
        private final Supplier<TabCreatorManager> mTabCreatorManagerSupplier;
        private final Supplier<Tab> mActiveTabSupplier;

        public Delegate(
                Supplier<TabCreatorManager> tabCreatorManagerSupplier,
                Supplier<Tab> activeTabSupplier) {
            mTabCreatorManagerSupplier = tabCreatorManagerSupplier;
            mActiveTabSupplier = activeTabSupplier;
        }

        /** Returns a {@link TabCreatorManager} used for creating the new tab. */
        @Nullable
        TabCreatorManager getTabCreatorManager() {
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
        @Nullable
        Supplier<Tab> getActiveTabSupplier() {
            return mActiveTabSupplier;
        }
    }

    /** Context used for fetching resources and window size. */
    private final Context mContext;

    private final Delegate mDelegate;
    private final Supplier<Tracker> mTrackerSupplier;

    private boolean mIsTablet;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    /**
     * Creates {@code OptionalNewTabButtonController}.
     *
     * @param context The Context for retrieving resources, etc.
     * @param buttonDrawable Drawable for the new tab button.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events, e.g.
     *         configuration changes.
     * @param tabCreatorManagerSupplier Used to open new tabs.
     * @param activeTabSupplier Used to access the current tab.
     * @param trackerSupplier  Supplier for the current profile tracker.
     */
    public OptionalNewTabButtonController(
            Context context,
            Drawable buttonDrawable,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            Supplier<TabCreatorManager> tabCreatorManagerSupplier,
            Supplier<Tab> activeTabSupplier,
            Supplier<Tracker> trackerSupplier) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                buttonDrawable,
                context.getString(R.string.button_new_tab),
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.NEW_TAB,
                /* tooltipTextResId= */ R.string.new_tab_title,
                /* showHoverHighlight= */ true);
        setShouldShowOnIncognitoTabs(true);

        mContext = context;
        mDelegate = new Delegate(tabCreatorManagerSupplier, activeTabSupplier);
        mTrackerSupplier = trackerSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);

        mIsTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
    }

    @Override
    public void onClick(View view) {
        Supplier<Tab> activeTabSupplier = mDelegate.getActiveTabSupplier();
        if (activeTabSupplier == null || activeTabSupplier.get() == null) return;

        TabCreatorManager tabCreatorManager = mDelegate.getTabCreatorManager();
        if (tabCreatorManager == null) return;

        boolean isIncognito = activeTabSupplier.get().isIncognito();
        RecordUserAction.record("MobileTopToolbarOptionalButtonNewTab");
        tabCreatorManager.getTabCreator(isIncognito).launchNtp();

        if (mTrackerSupplier.hasValue()) {
            mTrackerSupplier
                    .get()
                    .notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_NEW_TAB_OPENED);
        }
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        if (mIsTablet == isTablet) {
            return;
        }
        mIsTablet = isTablet;

        mButtonData.setCanShow(shouldShowButton(mActiveTabSupplier.get()));
    }

    @Override
    protected boolean shouldShowButton(Tab tab) {
        if (!super.shouldShowButton(tab) || mIsTablet) return false;

        if (UrlUtilities.isNtpUrl(tab.getUrl())) return false;

        return true;
    }

    /**
     * Returns an IPH for this button. Only called once native is initialized and when {@code
     * AdaptiveToolbarFeatures.isCustomizationEnabled()} is true.
     * @param tab Current tab.
     */
    @Override
    protected IPHCommandBuilder getIphCommandBuilder(Tab tab) {
        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
        IPHCommandBuilder iphCommandBuilder =
                new IPHCommandBuilder(
                                tab.getContext().getResources(),
                                FeatureConstants
                                        .ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_NEW_TAB_FEATURE,
                                /* stringId= */ R.string.adaptive_toolbar_button_new_tab_iph,
                                /* accessibilityStringId= */ R.string
                                        .adaptive_toolbar_button_new_tab_iph)
                        .setHighlightParams(params);
        return iphCommandBuilder;
    }
}
