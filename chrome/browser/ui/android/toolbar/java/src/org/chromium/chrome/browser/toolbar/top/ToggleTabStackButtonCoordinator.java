// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;

import androidx.annotation.VisibleForTesting;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.util.XrUtils;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/**
 * Root component for the tab switcher button on the toolbar. Intended to own the {@link
 * ToggleTabStackButton}, but currently it only manages some signals around the tab switcher button.
 * TODO(crbug.com/40588354): Finish converting HomeButton to MVC and move more logic into this
 * class.
 */
@NullMarked
public class ToggleTabStackButtonCoordinator extends ToolbarChild {
    private static final int IPH_TAB_SWITCHER_XR_WAIT_TIME_MS = 5 * 1000;
    private static final int IPH_TAB_SWITCHER_XR_MIN_TABS = 4;
    private static final long ONE_DAY_IN_MILLIS = TimeUnit.DAYS.toMillis(1);
    private final CallbackController mCallbackController = new CallbackController();
    private final Context mContext;
    private final ToggleTabStackButton mToggleTabStackButton;
    private final UserEducationHelper mUserEducationHelper;
    private final OneshotSupplier<Boolean> mPromoShownOneshotSupplier;
    private final CurrentTabObserver mPageLoadObserver;
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Callback<Integer> mTabCountSupplierObserver = this::onUpdateTabCount;
    private final Callback<TabModelDotInfo> mNotificationDotObserver =
            this::onUpdateNotificationDot;
    private @Nullable ObservableSupplier<Integer> mTabCountSupplier;
    private @Nullable ObservableSupplier<TabModelDotInfo> mNotificationDotSupplier;

    private @Nullable LayoutStateProvider mLayoutStateProvider;
    private @Nullable LayoutStateObserver mLayoutStateObserver;
    @VisibleForTesting boolean mIphBeingShown;
    // Non-null when tab declutter is enabled and initWithNative is called.
    private @Nullable ObservableSupplier<Integer> mArchivedTabCountSupplier;
    private @Nullable Runnable mArchivedTabsIphShownCallback;
    private @Nullable Runnable mArchivedTabsIphDismissedCallback;
    private final Callback<Integer> mArchivedTabCountObserver = this::maybeShowDeclutterIph;
    private @Nullable Callback<TabModelSelector> mTabModelSelectorCallback;
    private boolean mAlreadyRequestedDeclutterIph;
    private long mLastTimeXrIphWasShown;

    /**
     * @param context The Android context used for various view operations.
     * @param toggleTabStackButton The concrete {@link ToggleTabStackButton} class for this MVC
     *     component.
     * @param userEducationHelper Helper class for showing in-product help text bubbles.
     * @param promoShownOneshotSupplier Potentially delayed information about if a promo was shown.
     * @param layoutStateProviderSupplier Allows observing layout state.
     * @param activityTabSupplier Supplier of the activity tab.
     * @param tabModelSelectorSupplier Supplier for @{@link TabModelSelector}.
     */
    public ToggleTabStackButtonCoordinator(
            Context context,
            ToggleTabStackButton toggleTabStackButton,
            UserEducationHelper userEducationHelper,
            OneshotSupplier<Boolean> promoShownOneshotSupplier,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            ObservableSupplier<@Nullable Tab> activityTabSupplier,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            ThemeColorProvider themeColorProvider,
            IncognitoStateProvider incognitoStateProvider) {
        super(themeColorProvider, incognitoStateProvider);
        mContext = context;
        mToggleTabStackButton = toggleTabStackButton;
        mUserEducationHelper = userEducationHelper;
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
     * @param tabModelNotificationDotSupplier Supplies whether to show the notification dot on the
     *     tab switcher button.
     * @param archivedTabsIphShownCallback Callback for when the archived tabs iph is shown.
     * @param archivedTabsIphDismissedCallback Callback for when the archived tabs iph is dismissed.
     */
    public void initializeWithNative(
            OnClickListener onClickListener,
            OnLongClickListener onLongClickListener,
            ObservableSupplier<Integer> tabCountSupplier,
            @Nullable ObservableSupplier<Integer> archivedTabCountSupplier,
            ObservableSupplier<TabModelDotInfo> tabModelNotificationDotSupplier,
            Runnable archivedTabsIphShownCallback,
            Runnable archivedTabsIphDismissedCallback) {
        mTabCountSupplier = tabCountSupplier;
        if (mTabCountSupplier != null) {
            mTabCountSupplier.addObserver(mTabCountSupplierObserver);
        }

        mNotificationDotSupplier = tabModelNotificationDotSupplier;
        if (mNotificationDotSupplier != null) {
            mNotificationDotSupplier.addObserver(mNotificationDotObserver);
        }

        mToggleTabStackButton.setOnClickListener(onClickListener);
        mToggleTabStackButton.setOnLongClickListener(onLongClickListener);
        mToggleTabStackButton.setSuppliers(tabCountSupplier);

        mArchivedTabCountSupplier = archivedTabCountSupplier;
        if (mArchivedTabCountSupplier != null) {
            mArchivedTabCountSupplier.addObserver(mArchivedTabCountObserver);
            mArchivedTabsIphShownCallback = archivedTabsIphShownCallback;
            mArchivedTabsIphDismissedCallback = archivedTabsIphDismissedCallback;
        }

        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        TabModelUtils.runOnTabStateInitialized(
                tabModelSelector,
                mCallbackController.makeCancelable(
                        (unusedTabModelSelector) -> {
                            handleTabRestoreCompleted();
                        }));
        if (tabModelSelector.isTabStateInitialized()) {
            handleTabRestoreCompleted();
        }
    }

    /** Cleans up callbacks and observers. */
    @Override
    public void destroy() {
        super.destroy();
        mCallbackController.destroy();

        mPageLoadObserver.destroy();

        if (mTabCountSupplier != null) {
            mTabCountSupplier.removeObserver(mTabCountSupplierObserver);
        }
        if (mTabModelSelectorCallback != null) {
            mTabModelSelectorSupplier.removeObserver(mTabModelSelectorCallback);
        }

        if (mNotificationDotSupplier != null) {
            mNotificationDotSupplier.removeObserver(mNotificationDotObserver);
        }

        if (mLayoutStateProvider != null) {
            assumeNonNull(mLayoutStateObserver);
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
     */
    @Override
    public void draw(View root, Canvas canvas) {
        canvas.save();
        ViewUtils.translateCanvasToView(root, mToggleTabStackButton, canvas);
        mToggleTabStackButton.drawTabSwitcherAnimationOverlay(canvas);
        canvas.restore();
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        if (mToggleTabStackButton == null) return;
        mToggleTabStackButton.setIncognitoState(isIncognito);
        // Set the correct branded color scheme tinting for the {@link TabSwitcherDrawable} whenever
        // the incognito state changes.
        mToggleTabStackButton.setBrandedColorScheme(
                mTopUiThemeColorProvider.getBrandedColorScheme());
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        super.onTintChanged(tint, activityFocusTint, brandedColorScheme);
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            ImageViewCompat.setImageTintList(mToggleTabStackButton, activityFocusTint);
        } else {
            mToggleTabStackButton.setBrandedColorScheme(brandedColorScheme);
        }
    }

    private void handleTabRestoreCompleted() {
        // Enable tab switcher button.
        mToggleTabStackButton.setClickable(true);
    }

    /** Update button with branded color scheme. */
    public void setBrandedColorScheme(int brandedColorScheme) {
        mToggleTabStackButton.setBrandedColorScheme(brandedColorScheme);
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
        IphCommandBuilder builder = null;
        if (ChromeFeatureList.sTabStripIncognitoMigration.isEnabled()
                && mTabModelSelectorSupplier.hasValue()) {
            TabModelSelector selector = mTabModelSelectorSupplier.get();
            // When in Incognito, show IPH to switch out.
            if (selector.getCurrentModel().isIncognitoBranded()) {
                builder =
                        new IphCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.TAB_SWITCHER_BUTTON_SWITCH_INCOGNITO,
                                R.string.iph_tab_switcher_switch_out_of_incognito_text,
                                R.string
                                        .iph_tab_switcher_switch_out_of_incognito_accessibility_text);
            } else if (selector.getModel(true).getCount() > 0) {
                // When in standard model with incognito tabs, show IPH to switch into incognito.
                builder =
                        new IphCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.TAB_SWITCHER_BUTTON_SWITCH_INCOGNITO,
                                R.string.iph_tab_switcher_switch_into_incognito_text,
                                R.string.iph_tab_switcher_switch_into_incognito_accessibility_text);
            }
        }

        if (builder == null
                && !mIncognitoStateProvider.isIncognitoSelected()
                && mPromoShownOneshotSupplier.hasValue()
                && !mPromoShownOneshotSupplier.get()) {
            builder =
                    new IphCommandBuilder(
                            mContext.getResources(),
                            FeatureConstants.TAB_SWITCHER_BUTTON_FEATURE,
                            R.string.iph_tab_switcher_text,
                            R.string.iph_tab_switcher_accessibility_text);
        }

        if (builder != null) {
            mUserEducationHelper.requestShowIph(
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

    private void onUpdateTabCount(int tabCount) {
        mToggleTabStackButton.setEnabled(tabCount >= 1);
        mToggleTabStackButton.updateTabCount(
                tabCount, mIncognitoStateProvider.isIncognitoSelected());
        mToggleTabStackButton.setBrandedColorScheme(
                mTopUiThemeColorProvider.getBrandedColorScheme());
        maybeShowXrIph(tabCount);
    }

    private void onUpdateNotificationDot(TabModelDotInfo tabModelDotInfo) {
        mToggleTabStackButton.onUpdateNotificationDot(tabModelDotInfo);
        if (tabModelDotInfo.showDot && mUserEducationHelper != null) {
            String tabGroupTitle = tabModelDotInfo.tabGroupTitle;
            String contentString =
                    mContext.getString(R.string.tab_group_update_iph_text, tabGroupTitle);
            mUserEducationHelper.requestShowIph(
                    new IphCommandBuilder(
                                    mContext.getResources(),
                                    FeatureConstants.TAB_GROUP_SHARE_UPDATE_FEATURE,
                                    contentString,
                                    contentString)
                            .setAnchorView(mToggleTabStackButton)
                            .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                            .build());
        }
    }

    private void maybeShowDeclutterIph(int tabCount) {
        if (mIncognitoStateProvider.isIncognitoSelected()) return;
        if (mAlreadyRequestedDeclutterIph) return;
        if (tabCount == 0) return;
        mAlreadyRequestedDeclutterIph = true;
        HighlightParams params = new HighlightParams(HighlightShape.CIRCLE);
        params.setBoundsRespectPadding(true);
        assumeNonNull(mArchivedTabsIphShownCallback);
        assumeNonNull(mArchivedTabsIphDismissedCallback);
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
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

    private void maybeShowXrIph(int tabCount) {
        if (!XrUtils.isXrDevice()) return;
        if (tabCount < IPH_TAB_SWITCHER_XR_MIN_TABS) return;
        if (mUserEducationHelper == null) return;

        long currentTime = System.currentTimeMillis();

        // We don't show the IPH again unless Chrome is fully restarted
        // or one day has elapsed since last time it was dismissed.
        if (currentTime - mLastTimeXrIphWasShown < ONE_DAY_IN_MILLIS) return;

        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mContext.getResources(),
                                FeatureConstants.IPH_TAB_SWITCHER_XR,
                                R.string.iph_tab_switcher_xr,
                                R.string.iph_tab_switcher_xr)
                        .setAnchorView(mToggleTabStackButton)
                        .setAutoDismissTimeout(IPH_TAB_SWITCHER_XR_WAIT_TIME_MS)
                        .setOnDismissCallback(
                                () -> {
                                    mLastTimeXrIphWasShown = System.currentTimeMillis();
                                })
                        .build());
    }
}
