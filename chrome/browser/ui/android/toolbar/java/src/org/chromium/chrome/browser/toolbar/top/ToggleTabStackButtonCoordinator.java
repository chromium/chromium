// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

/**
 * Root component for the tab switcher button on the toolbar. Intended to own the {@link
 * ToggleTabStackButton}, but currently it only manages some signals around the tab switcher button.
 * TODO(crbug.com/40588354): Finish converting HomeButton to MVC and move more logic into this
 * class.
 */
public class ToggleTabStackButtonCoordinator {
    private final CallbackController mCallbackController = new CallbackController();
    private final Context mContext;
    @NonNull private ToggleTabStackButton mToggleTabStackButton;
    private final UserEducationHelper mUserEducationHelper;
    private final Supplier<Boolean> mIsIncognitoSupplier;
    private final OneshotSupplier<Boolean> mPromoShownOneshotSupplier;
    private final CurrentTabObserver mPageLoadObserver;
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;

    private LayoutStateProvider mLayoutStateProvider;
    private LayoutStateObserver mLayoutStateObserver;
    @VisibleForTesting boolean mIphBeingShown;
    // Non-null when tab declutter is enabled and initWithNative is called.
    private @Nullable ObservableSupplier<Integer> mArchivedTabCountSupplier;
    private @Nullable Runnable mArchivedTabsIphShownCallback;
    private @Nullable Runnable mArchivedTabsIphDismissedCallback;
    private @Nullable Callback<Integer> mArchivedTabCountObserver = this::maybeShowDeclutterIph;

    /**
     * @param context The Android context used for various view operations.
     * @param toggleTabStackButton The concrete {@link ToggleTabStackButton} class for this MVC
     *     component.
     * @param userEducationHelper Helper class for showing in-product help text bubbles.
     * @param isIncognitoSupplier Supplier for whether the current tab is incognito.
     * @param promoShownOneshotSupplier Potentially delayed information about if a promo was shown.
     * @param layoutStateProviderSupplier Allows observing layout state.
     * @param activityTabSupplier Supplier of the activity tab.
     * @param tabModelSelectorSupplier Supplier for @{@link TabModelSelector}.
     */
    public ToggleTabStackButtonCoordinator(
            Context context,
            ToggleTabStackButton toggleTabStackButton,
            UserEducationHelper userEducationHelper,
            Supplier<Boolean> isIncognitoSupplier,
            OneshotSupplier<Boolean> promoShownOneshotSupplier,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            ObservableSupplier<Tab> activityTabSupplier,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mContext = context;
        mToggleTabStackButton = toggleTabStackButton;
        mUserEducationHelper = userEducationHelper;
        mIsIncognitoSupplier = isIncognitoSupplier;
        mPromoShownOneshotSupplier = promoShownOneshotSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;

        layoutStateProviderSupplier.onAvailable(
                mCallbackController.makeCancelable(this::setLayoutStateProvider));

        mPageLoadObserver =
                new CurrentTabObserver(
                        activityTabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onPageLoadFinished(Tab tab, GURL url) {
                                handlePageLoadFinished();
                            }
                        },
                        /* swapCallback= */ null);
    }

    /**
     * Post native initializations.
     *
     * @param onClickListener OnClickListener for view.
     * @param onLongClickListener OnLongClickListener for view.
     * @param tabCountSupplier Supplier for current tab count to show in view.
     * @param archivedTabCountSupplier Supplies the current archived tab count, used for displaying
     *     the associated IPH.
     * @param archivedTabsIphShownCallback Callback for when the archived tabs iph is shown.
     */
    public void initializeWithNative(
            OnClickListener onClickListener,
            OnLongClickListener onLongClickListener,
            ObservableSupplier<Integer> tabCountSupplier,
            @Nullable ObservableSupplier<Integer> archivedTabCountSupplier,
            @NonNull Runnable archivedTabsIphShownCallback,
            @NonNull Runnable archivedTabsIphDismissedCallback) {
        mToggleTabStackButton.setOnClickListener(onClickListener);
        mToggleTabStackButton.setOnLongClickListener(onLongClickListener);
        mToggleTabStackButton.setTabCountSupplier(tabCountSupplier, mIsIncognitoSupplier);

        mArchivedTabCountSupplier = archivedTabCountSupplier;
        if (mArchivedTabCountSupplier != null) {
            mArchivedTabCountSupplier.addObserver(mArchivedTabCountObserver);
            mArchivedTabsIphShownCallback = archivedTabsIphShownCallback;
            mArchivedTabsIphDismissedCallback = archivedTabsIphDismissedCallback;
        }
    }

    /** Cleans up callbacks and observers. */
    public void destroy() {
        mCallbackController.destroy();

        mPageLoadObserver.destroy();

        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
            mLayoutStateProvider = null;
            mLayoutStateObserver = null;
        }

        if (mArchivedTabCountSupplier != null) {
            mArchivedTabCountSupplier.removeObserver(mArchivedTabCountObserver);
        }

        mToggleTabStackButton.destroy();
    }

    /** Get container view for drawing, accessibility traversal and animations. */
    public View getContainerView() {
        return mToggleTabStackButton;
    }

    /**
     * Draws the current visual state of this component for the purposes of rendering the tab
     * switcher animation, setting the alpha to fade the view by the appropriate amount.
     *
     * @param root Root view for the menu button; used to position the canvas that's drawn on.
     * @param canvas Canvas to draw to.
     * @param alpha Integer (0-255) alpha level to draw at.
     */
    public void drawTabSwitcherAnimationOverlay(View root, Canvas canvas, int alpha) {
        canvas.save();
        ViewUtils.translateCanvasToView(root, mToggleTabStackButton, canvas);
        mToggleTabStackButton.drawTabSwitcherAnimationOverlay(canvas, alpha);
        canvas.restore();
    }

    /** Get tab count on button for texture capture. */
    public int getDrawableTabCount() {
        return ((TabSwitcherDrawable) mToggleTabStackButton.getDrawable()).getTabCount();
    }

    /** Update button with branded color scheme. */
    public void setBrandedColorScheme(int brandedColorScheme) {
        mToggleTabStackButton.setBrandedColorScheme(brandedColorScheme);
    }

    public Supplier<Boolean> getIsIncognitoSupplier() {
        return mIsIncognitoSupplier;
    }

    private void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        assert layoutStateProvider != null;
        assert mLayoutStateProvider == null : "the mLayoutStateProvider should set at most once.";

        mLayoutStateProvider = layoutStateProvider;
        // Make button un-clickable during browser layout transition. Re-enable once transition
        // completes.
        mLayoutStateObserver =
                new LayoutStateObserver() {

                    @Override
                    public void onStartedShowing(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.BROWSING) {
                            setClickable(false);
                        } else if (layoutType == LayoutType.TAB_SWITCHER) {
                            updateTabSwitcherButtonRipple();
                        }
                    }

                    @Override
                    public void onStartedHiding(@LayoutType int layoutType) {
                        if (layoutType == LayoutType.BROWSING) {
                            setClickable(false);
                        }
                    }

                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if (layoutType == LayoutType.BROWSING) {
                            setClickable(true);
                        }
                    }

                    @Override
                    public void onFinishedHiding(int layoutType) {
                        if (layoutType == LayoutType.BROWSING) {
                            setClickable(true);
                        }
                    }
                };
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
    }

    private void setClickable(boolean val) {
        mToggleTabStackButton.setClickable(val);
    }

    @VisibleForTesting
    void handlePageLoadFinished() {
        if (!mToggleTabStackButton.isShown()) return;

        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
        IPHCommandBuilder builder = null;
        if (ChromeFeatureList.sTabStripIncognitoMigration.isEnabled()
                && mTabModelSelectorSupplier.hasValue()) {
            TabModelSelector selector = mTabModelSelectorSupplier.get();
            // When in Incognito, show IPH to switch out.
            if (selector.getCurrentModel().isIncognitoBranded()) {
                builder =
                        new IPHCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.TAB_SWITCHER_BUTTON_SWITCH_INCOGNITO,
                                R.string.iph_tab_switcher_switch_out_of_incognito_text,
                                R.string
                                        .iph_tab_switcher_switch_out_of_incognito_accessibility_text);
            } else if (selector.getModel(true).getCount() > 0) {
                // When in standard model with incognito tabs, show IPH to switch into incognito.
                builder =
                        new IPHCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.TAB_SWITCHER_BUTTON_SWITCH_INCOGNITO,
                                R.string.iph_tab_switcher_switch_into_incognito_text,
                                R.string.iph_tab_switcher_switch_into_incognito_accessibility_text);
            }
        } else if (!mIsIncognitoSupplier.get()
                && mPromoShownOneshotSupplier.hasValue()
                && !mPromoShownOneshotSupplier.get()) {
            builder =
                    new IPHCommandBuilder(
                            mContext.getResources(),
                            FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE,
                            R.string.iph_tab_switcher_text,
                            R.string.iph_tab_switcher_accessibility_text);
        }

        if (builder != null) {
            mUserEducationHelper.requestShowIPH(
                    builder.setAnchorView(mToggleTabStackButton)
                            .setOnShowCallback(this::handleShowCallback)
                            .setOnDismissCallback(this::handleDismissCallback)
                            .setHighlightParams(params)
                            .build());
        }
    }

    /**
     * Enables or disables the tab switcher ripple depending on whether we are in or out of the tab
     * switcher mode.
     */
    private void updateTabSwitcherButtonRipple() {
        Drawable drawable = mToggleTabStackButton.getBackground();
        // drawable may not be a RippleDrawable if IPH is showing. Ignore that scenario since
        // it is rare.
        if (drawable instanceof RippleDrawable) {
            // Force the ripple to end so the transition looks correct.
            drawable.jumpToCurrentState();
        }
    }

    private void handleShowCallback() {
        mIphBeingShown = true;
    }

    private void handleDismissCallback() {
        mIphBeingShown = false;
    }

    private void maybeShowDeclutterIph(int tabCount) {
        if (!ChromeFeatureList.sAndroidTabDeclutter.isEnabled()) return;
        if (mIsIncognitoSupplier.get()) return;
        if (tabCount == 0) return;

        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.ANDROID_TAB_DECLUTTER_FEATURE,
                                R.string.iph_android_tab_declutter_text,
                                R.string.iph_android_tab_declutter_accessibility_text)
                        .setAnchorView(mToggleTabStackButton)
                        .setHighlightParams(params)
                        .setOnShowCallback(mArchivedTabsIphShownCallback)
                        .setOnDismissCallback(mArchivedTabsIphDismissedCallback)
                        .build());
    }
}
