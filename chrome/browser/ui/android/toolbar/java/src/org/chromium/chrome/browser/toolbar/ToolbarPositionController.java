// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Class responsible for managing the position (top, bottom) of the browsing mode toolbar. */
public class ToolbarPositionController implements OnSharedPreferenceChangeListener {

    @IntDef({
        ToolbarPositionController.StateTransition.NONE,
        ToolbarPositionController.StateTransition.SNAP_TO_TOP,
        ToolbarPositionController.StateTransition.SNAP_TO_BOTTOM,
        ToolbarPositionController.StateTransition.ANIMATE_TO_TOP,
        ToolbarPositionController.StateTransition.ANIMATE_TO_BOTTOM,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface StateTransition {
        // Don't transition at all.
        int NONE = 0;
        // Snap (instantly transition) the controls to the top.
        int SNAP_TO_TOP = 1;
        // Snap (instantly transition) the controls to the bottom.
        int SNAP_TO_BOTTOM = 2;
        // Animate the controls to the top.
        int ANIMATE_TO_TOP = 3;
        // Animate the controls to the bottom.
        int ANIMATE_TO_BOTTOM = 4;
    }

    // User-configured, or, otherwise, default Toolbar placement; may be null, if target placement
    // has not been determined yet. Prefer `isToolbarConfiguredToShowOnTop()` call when querying
    // intended placement.
    private static Boolean sToolbarShouldShowOnTop;

    private final BrowserControlsSizer mBrowserControlsSizer;
    private final ObservableSupplier<Boolean> mIsNtpShowingSupplier;
    private final ObservableSupplier<Boolean> mIsTabSwitcherFinishedShowingSupplier;
    private final ObservableSupplier<Boolean> mIsOmniboxFocusedSupplier;
    private final ObservableSupplier<Boolean> mIsFormFieldFocusedSupplier;
    @NonNull private final ObservableSupplier<Boolean> mIsFindInPageShowingSupplier;
    private final ControlContainer mControlContainer;
    private final BottomControlsStacker mBottomControlsStacker;
    private final ObservableSupplierImpl<Integer> mBrowserControlsOffsetSupplier;
    @NonNull private final View mToolbarProgressBarContainer;
    @NonNull private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    @NonNull private final Context mContext;
    @LayerVisibility private int mLayerVisibility;
    private final BottomControlsLayer mBottomToolbarLayer;
    private final BottomControlsLayer mProgressBarLayer;

    @ControlsPosition private int mCurrentPosition;
    private int mHairlineHeight;

    /**
     * @param browserControlsSizer {@link BrowserControlsSizer}, used to manipulate position of the
     *     browser controls and relative heights of the top and bottom controls.
     * @param sharedPreferences SharedPreferences instance used to monitor user preference state.
     * @param isNtpShowingSupplier Supplier of the current state of the NTP. Must have a non-null
     *     value immediately available.
     * @param isTabSwitcherFinishedShowingSupplier Supplier indicating whether the tab switcher has
     *     finished showing. It should only reflect `true` once the transition animation has fully
     *     completed.
     * @param isOmniboxFocusedSupplier Supplier of the current omnibox focus state. Must have a
     *     non-null value immediately available.
     * @param isFormFieldFocusedSupplier Supplier of the current form field focus state for the
     *     active WebContents. Must have a non-null value immediately available.
     * @param isFindInPageShowingSupplier Supplier telling us if the "find in page" UI is showing.
     * @param controlContainer The control container for the current context.
     * @param bottomControlsStacker {@link BottomControlsStacker} used to harmonize the position of
     *     the bottom toolbar with other bottom-anchored UI.
     */
    public ToolbarPositionController(
            @NonNull BrowserControlsSizer browserControlsSizer,
            @NonNull SharedPreferences sharedPreferences,
            @NonNull ObservableSupplier<Boolean> isNtpShowingSupplier,
            @NonNull ObservableSupplier<Boolean> isTabSwitcherFinishedShowingSupplier,
            @NonNull ObservableSupplier<Boolean> isOmniboxFocusedSupplier,
            @NonNull ObservableSupplier<Boolean> isFormFieldFocusedSupplier,
            @NonNull ObservableSupplier<Boolean> isFindInPageShowingSupplier,
            @NonNull KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            @NonNull ControlContainer controlContainer,
            @NonNull BottomControlsStacker bottomControlsStacker,
            @NonNull ObservableSupplierImpl<Integer> browserControlsOffsetSupplier,
            @NonNull View toolbarProgressBarContainer,
            @NonNull Context context) {
        mBrowserControlsSizer = browserControlsSizer;
        mIsNtpShowingSupplier = isNtpShowingSupplier;
        mIsTabSwitcherFinishedShowingSupplier = isTabSwitcherFinishedShowingSupplier;
        mIsOmniboxFocusedSupplier = isOmniboxFocusedSupplier;
        mIsFormFieldFocusedSupplier = isFormFieldFocusedSupplier;
        mIsFindInPageShowingSupplier = isFindInPageShowingSupplier;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mControlContainer = controlContainer;
        mBottomControlsStacker = bottomControlsStacker;
        mBrowserControlsOffsetSupplier = browserControlsOffsetSupplier;
        mToolbarProgressBarContainer = toolbarProgressBarContainer;
        mContext = context;
        mCurrentPosition = mBrowserControlsSizer.getControlsPosition();

        mHairlineHeight =
                context.getResources().getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        mIsNtpShowingSupplier.addObserver((showing) -> updateCurrentPosition());
        mIsTabSwitcherFinishedShowingSupplier.addObserver((showing) -> updateCurrentPosition());
        mIsOmniboxFocusedSupplier.addObserver((focused) -> updateCurrentPosition());
        mIsFormFieldFocusedSupplier.addObserver(
                (focused) -> updateCurrentPosition(/* formFieldStateChanged= */ true, false));
        mIsFindInPageShowingSupplier.addObserver((showing) -> updateCurrentPosition());
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(
                (showing) -> updateCurrentPosition(/* formFieldStateChanged= */ true, false));
        sharedPreferences.registerOnSharedPreferenceChangeListener(this);
        recordStartupPosition(isToolbarConfiguredToShowOnTop());

        mLayerVisibility = LayerVisibility.HIDDEN;
        mBottomToolbarLayer =
                new BottomControlsLayer() {
                    @Override
                    public int getType() {
                        return LayerType.BOTTOM_TOOLBAR;
                    }

                    @Override
                    public int getScrollBehavior() {
                        return LayerScrollBehavior.DEFAULT_SCROLL_OFF;
                    }

                    @Override
                    public int getHeight() {
                        return mControlContainer.getToolbarHeight();
                    }

                    @Override
                    public int getLayerVisibility() {
                        return mLayerVisibility;
                    }

                    @Override
                    public void onBrowserControlsOffsetUpdate(int layerYOffset) {
                        if (mLayerVisibility == LayerVisibility.VISIBLE) {
                            mBrowserControlsOffsetSupplier.set(layerYOffset);
                            mControlContainer.getView().setTranslationY(layerYOffset);
                        }
                    }
                };
        mProgressBarLayer =
                new BottomControlsLayer() {
                    @Override
                    public int getType() {
                        return LayerType.PROGRESS_BAR;
                    }

                    @Override
                    public int getScrollBehavior() {
                        return LayerScrollBehavior.DEFAULT_SCROLL_OFF;
                    }

                    @Override
                    public int getHeight() {
                        return 0;
                    }

                    @Override
                    public int getLayerVisibility() {
                        return mLayerVisibility;
                    }

                    @Override
                    public void onBrowserControlsOffsetUpdate(int layerYOffset) {
                        mToolbarProgressBarContainer.setTranslationY(layerYOffset);
                    }
                };

        mBottomControlsStacker.addLayer(mBottomToolbarLayer);
        mBottomControlsStacker.addLayer(mProgressBarLayer);
        updateCurrentPosition();
    }

    /**
     * Returns whether the given {context, device, cct-ness} combo is eligible for toolbar position
     * customization.
     *
     * <p>NOTE: this method controls whether feature can take effect, and is separate from code
     * controlling whether feature can be configured - {@see
     * org.chromium.chrome.browser.settings.MainSettings#updateAddressBarPreference()}.
     */
    public static boolean isToolbarPositionCustomizationEnabled(
            Context context, boolean isCustomTab) {
        return !isCustomTab
                && ChromeFeatureList.sAndroidBottomToolbar.isEnabled()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /**
     * Returns the resource ID of a string representing the toolbar's position..
     *
     * <p>This method returns the resource ID for a string that indicates the toolbar's position
     * within the UI. The string value corresponding to the returned resource ID will typically be
     * "Top" or "Bottom", representing the toolbar's placement.
     *
     * @return The resource ID of the string indicating the toolbar's position.
     */
    public static int getToolbarPositionResId() {
        return isToolbarConfiguredToShowOnTop()
                ? R.string.address_bar_settings_top
                : R.string.address_bar_settings_bottom;
    }

    @Override
    public void onSharedPreferenceChanged(
            SharedPreferences sharedPreferences, @Nullable String key) {
        if (ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED.equals(key)) {
            // Re-set placement to retrieve it from prefs upon next access.
            sToolbarShouldShowOnTop = null;
            recordPrefChange(isToolbarConfiguredToShowOnTop());
            updateCurrentPosition(false, /* prefStateChanged= */ true);
        }
    }

    /** Returns true if toolbar is user-configured to show on top. */
    private static boolean isToolbarConfiguredToShowOnTop() {
        if (sToolbarShouldShowOnTop == null) {
            sToolbarShouldShowOnTop =
                    ContextUtils.getAppSharedPreferences()
                            .getBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);
        }
        return sToolbarShouldShowOnTop;
    }

    private void updateCurrentPosition() {
        updateCurrentPosition(false, false);
    }

    private void updateCurrentPosition(boolean formFieldStateChanged, boolean prefStateChanged) {
        boolean ntpShowing = mIsNtpShowingSupplier.get();
        boolean tabSwitcherShowing = mIsTabSwitcherFinishedShowingSupplier.get();
        boolean isOmniboxFocused = mIsOmniboxFocusedSupplier.get();
        boolean isFindInPageShowing = mIsFindInPageShowingSupplier.get();
        boolean isFormFieldFocusedWithKeyboardVisible =
                mIsFormFieldFocusedSupplier.get()
                        && mKeyboardVisibilityDelegate.isKeyboardShowing(
                                mContext, mControlContainer.getView());
        @StateTransition
        int stateTransition =
                calculateStateTransition(
                        formFieldStateChanged,
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        isToolbarConfiguredToShowOnTop(),
                        mCurrentPosition);
        @ControlsPosition
        int newControlsPosition =
                switch (stateTransition) {
                    case StateTransition.SNAP_TO_BOTTOM,
                            StateTransition.ANIMATE_TO_BOTTOM -> ControlsPosition.BOTTOM;
                    case StateTransition.SNAP_TO_TOP,
                            StateTransition.ANIMATE_TO_TOP -> ControlsPosition.TOP;
                    case StateTransition.NONE -> mCurrentPosition;
                    default -> mCurrentPosition;
                };

        if (newControlsPosition == mCurrentPosition) return;

        int newTopHeight;
        int controlContainerHeight = mControlContainer.getToolbarHeight();

        if (newControlsPosition == ControlsPosition.TOP) {
            newTopHeight = mBrowserControlsSizer.getTopControlsHeight() + controlContainerHeight;
            mLayerVisibility = LayerVisibility.HIDDEN;
            mControlContainer.getView().setTranslationY(0);
            mToolbarProgressBarContainer.setTranslationY(0);
            CoordinatorLayout.LayoutParams progressBarLayoutParams =
                    (LayoutParams) mToolbarProgressBarContainer.getLayoutParams();
            progressBarLayoutParams.setAnchorId(mControlContainer.getView().getId());
            progressBarLayoutParams.anchorGravity = Gravity.BOTTOM;
            progressBarLayoutParams.gravity = Gravity.TOP;
        } else {
            newTopHeight = mBrowserControlsSizer.getTopControlsHeight() - controlContainerHeight;
            mLayerVisibility = LayerVisibility.VISIBLE;
            CoordinatorLayout.LayoutParams progressBarLayoutParams =
                    (LayoutParams) mToolbarProgressBarContainer.getLayoutParams();
            progressBarLayoutParams.setAnchorId(View.NO_ID);
            progressBarLayoutParams.anchorGravity = Gravity.NO_GRAVITY;
            progressBarLayoutParams.gravity = Gravity.BOTTOM;
        }

        boolean animatingToTop = stateTransition == StateTransition.ANIMATE_TO_TOP;
        boolean animatingToBottom = stateTransition == StateTransition.ANIMATE_TO_BOTTOM;

        mBottomControlsStacker.updateLayerVisibilitiesAndSizes();
        mCurrentPosition = newControlsPosition;
        if (animatingToTop || animatingToBottom) {
            mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(true);
            // Prevent a visual glitch when animating the control container into a new location by
            // making it immediately invisible. Without this, it can show for a single frame before
            // hiding then sliding into place.
            mControlContainer.getView().setVisibility(View.INVISIBLE);
        }

        mBrowserControlsSizer.setControlsPosition(
                mCurrentPosition,
                newTopHeight,
                mBrowserControlsSizer.getTopControlsMinHeight(),
                // If animating to top, set the initial offset of the animation to fully hide the
                // toolbar. This is negative since it's relative to the top of the content.
                animatingToTop
                        ? -controlContainerHeight
                        : mBrowserControlsSizer.getTopControlOffset(),
                mBottomControlsStacker.getTotalHeight(),
                mBottomControlsStacker.getTotalMinHeight(),
                // If animating to bottom, set the initial offset of the animation to fully hide the
                // toolbar. This is positive since it's relative to the bottom of the content.
                animatingToBottom
                        ? controlContainerHeight
                        : mBrowserControlsSizer.getBottomControlOffset());
        mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(false);

        // Commit the new layer sizes and visibilities we calculated above to avoid inconsistency.
        mBottomControlsStacker.requestLayerUpdate(false);
        FrameLayout.LayoutParams hairlineLayoutParams =
                mControlContainer.mutateHairlineLayoutParams();
        hairlineLayoutParams.topMargin =
                mCurrentPosition == ControlsPosition.TOP ? controlContainerHeight : 0;
        hairlineLayoutParams.bottomMargin =
                mCurrentPosition == ControlsPosition.BOTTOM ? controlContainerHeight : 0;
        CoordinatorLayout.LayoutParams layoutParams = mControlContainer.mutateLayoutParams();
        int verticalGravity =
                mCurrentPosition == ControlsPosition.TOP ? Gravity.TOP : Gravity.BOTTOM;
        layoutParams.gravity = Gravity.START | verticalGravity;
        FrameLayout.LayoutParams toolbarLayoutParams =
                mControlContainer.mutateToolbarLayoutParams();
        toolbarLayoutParams.topMargin =
                mCurrentPosition == ControlsPosition.BOTTOM ? mHairlineHeight : 0;
    }

    @VisibleForTesting
    static @StateTransition int calculateStateTransition(
            boolean formFieldStateChanged,
            boolean prefStateChanged,
            boolean ntpShowing,
            boolean tabSwitcherShowing,
            boolean isOmniboxFocused,
            boolean isFindInPageShowing,
            boolean isFormFieldFocusedWithKeyboardVisible,
            boolean doesUserPreferTopToolbar,
            @ControlsPosition int currentPosition) {
        @ControlsPosition int newControlsPosition;
        if (ntpShowing
                || tabSwitcherShowing
                || isOmniboxFocused
                || isFindInPageShowing
                || isFormFieldFocusedWithKeyboardVisible
                || doesUserPreferTopToolbar) {
            newControlsPosition = ControlsPosition.TOP;
        } else {
            newControlsPosition = ControlsPosition.BOTTOM;
        }

        boolean switchingToBottom = newControlsPosition == ControlsPosition.BOTTOM;
        if (newControlsPosition == currentPosition) {
            // Don't do anything for non-transitions.
            return StateTransition.NONE;
        } else if (formFieldStateChanged || prefStateChanged) {
            // Animate when the pref changes (i.e. the long press menu is invoked) or the keyboard
            // shows/hides.
            return switchingToBottom
                    ? StateTransition.ANIMATE_TO_BOTTOM
                    : StateTransition.ANIMATE_TO_TOP;
        }

        // For all other state transitions, just snap to the correct position immediately.
        return switchingToBottom ? StateTransition.SNAP_TO_BOTTOM : StateTransition.SNAP_TO_TOP;
    }

    /** Returns whether the toolbar will be shown on top for the supplied tab. */
    public static boolean shouldShowToolbarOnTop(Tab tab) {
        boolean isRegularNtp =
                (tab != null)
                        && (tab.getUrl() != null)
                        && UrlUtilities.isNtpUrl(tab.getUrl())
                        && !tab.isIncognitoBranded();

        return calculateStateTransition(
                        /* formFieldStateChanged= */ false,
                        /* prefStateChanged= */ false,
                        /* ntpShowing= */ isRegularNtp,
                        /* tabSwitcherShowing= */ false,
                        /* isOmniboxFocused= */ false,
                        /* isFindInPageShowing= */ false,
                        /* isFormFieldFocusedWithKeyboardVisible= */ false,
                        isToolbarConfiguredToShowOnTop(),
                        /* currentPosition= */ ControlsPosition.NONE)
                == StateTransition.SNAP_TO_TOP;
    }

    static void resetCachedToolbarConfigurationForTesting() {
        sToolbarShouldShowOnTop = null;
    }

    private static void recordStartupPosition(boolean userPrefersTop) {
        int sample = userPrefersTop ? ControlsPosition.TOP : ControlsPosition.BOTTOM;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.ToolbarPosition.PositionAtStartup", sample, ControlsPosition.NONE);
    }

    private static void recordPrefChange(boolean userPrefersTop) {
        int sample = userPrefersTop ? ControlsPosition.TOP : ControlsPosition.BOTTOM;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.ToolbarPosition.PositionPrefChanged", sample, ControlsPosition.NONE);
    }
}
