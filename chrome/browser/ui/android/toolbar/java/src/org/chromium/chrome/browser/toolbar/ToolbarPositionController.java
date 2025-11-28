// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.toolbar.settings.AddressBarPreference.computeToolbarPositionAndSource;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Handler;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.edge_to_edge.TopInsetCoordinator;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.settings.AddressBarPreference;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Class responsible for managing the position (top, bottom) of the browsing mode toolbar. */
@NullMarked
public class ToolbarPositionController implements OnSharedPreferenceChangeListener {

    private final Callback<Integer> mKeyboardAccessoryHeightObserver;

    @IntDef({
        ToolbarPositionAndSource.TOP_LONG_PRESS,
        ToolbarPositionAndSource.TOP_SETTINGS,
        ToolbarPositionAndSource.BOTTOM_LONG_PRESS,
        ToolbarPositionAndSource.BOTTOM_SETTINGS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ToolbarPositionAndSource {
        int TOP_LONG_PRESS = 0;
        int TOP_SETTINGS = 1;
        int BOTTOM_LONG_PRESS = 2;
        int BOTTOM_SETTINGS = 3;
        int UNDEFINED = -1;
    }

    @IntDef({
        StateTransition.NONE,
        StateTransition.SNAP_TO_TOP,
        StateTransition.SNAP_TO_BOTTOM,
        StateTransition.ANIMATE_TO_TOP,
        StateTransition.ANIMATE_TO_BOTTOM,
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

    /** Bottom controls layer that remembers its most recent offset */
    interface BottomControlsLayerWithOffset extends BottomControlsLayer {
        int getLayerOffsetPx();
    }

    // LINT.IfChange(TipsPrefNames)
    // Whether the bottom omnibox was ever used.
    public static final String BOTTOM_OMNIBOX_EVER_USED_PREF = "omnibox.bottom_omnibox_ever_used";
    // LINT.ThenChange(//components/omnibox/browser/omnibox_pref_names.h:TipsPrefNames)

    // User-configured, or, otherwise, default Toolbar placement; may be null, if target placement
    // has not been determined yet. Prefer `isToolbarConfiguredToShowOnTop()` call when querying
    // intended placement.
    private static @Nullable Boolean sToolbarShouldShowOnTop;

    private final BrowserControlsSizer mBrowserControlsSizer;
    private final ObservableSupplier<Boolean> mIsNtpWithFakeboxShowingSupplier;
    private final ObservableSupplier<Boolean> mIsIncognitoNtpShowingSupplier;
    private final ObservableSupplier<Boolean> mIsTabSwitcherFinishedShowingSupplier;
    private final ObservableSupplier<Boolean> mIsOmniboxFocusedSupplier;
    private final ObservableSupplier<Boolean> mIsFormFieldFocusedSupplier;
    private final ObservableSupplier<Boolean> mIsFindInPageShowingSupplier;
    private final ControlContainer mControlContainer;
    private final ToolbarLayout mToolbarLayout;
    private final BottomControlsStacker mBottomControlsStacker;
    private final ObservableSupplierImpl<Integer> mBrowserControlsOffsetSupplier;
    private final View mToolbarProgressBarContainer;
    private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final ObservableSupplier<Integer> mKeyboardAccessoryHeightSupplier;
    private final ObservableSupplier<Integer> mControlContainerTranslationSupplier;
    private final ObservableSupplier<Integer> mControlContainerHeightSupplier;
    private final ObservableSupplier<TopInsetCoordinator> mTopInsetCoordinatorSupplier;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Handler mHandler;
    @LayerVisibility private int mLayerVisibility;
    private int mControlContainerHeight;
    private final BottomControlsLayerWithOffset mBottomToolbarLayer;
    private final BottomControlsLayerWithOffset mProgressBarLayer;

    private final Callback<Boolean> mIsNtpShowingObserver;
    private final Callback<Boolean> mIsTabSwitcherFinishedShowingObserver;
    private final Callback<Boolean> mIsOmniboxFocusedObserver;
    private final Callback<Boolean> mIsFormFieldFocusedObserver;
    private final Callback<Boolean> mIsFindInPageShowingObserver;
    private final KeyboardVisibilityListener mKeyboardVisibilityListener;
    private final Callback<Integer> mKeyboardHeightToolbarCallback;
    private final Callback<Integer> mKeyboardHeightProgressBarCallback;
    private final KeyboardVisibilityListener mKeyboardVisibilityViewOffsetCallback;
    private final Callback<Boolean> mFormFieldViewOffsetCallback;
    private final Callback<Boolean> mIncognitoNtpShowingViewOffsetCallback;
    private final Callback<Integer> mControlContainerTranslationCallback;
    private final Callback<Integer> mControlContainerHeightCallback;
    private final SharedPreferences mSharedPreferences;
    private @Nullable Callback<TopInsetCoordinator> mTopInsetCoordinatorAvailableCallback;
    private TopInsetCoordinator.@Nullable Observer mTopInsetCoordinatorObserver;
    private int mTopInset;

    ObservableSupplierImpl<@ControlsPosition Integer> mCurrentPosition;
    private final ObservableSupplier<Integer> mKeyboardHeightSupplier;
    private final WindowAndroid mWindowAndroid;
    private final int mHairlineHeight;

    /**
     * @param browserControlsSizer {@link BrowserControlsSizer}, used to manipulate position of the
     *     browser controls and relative heights of the top and bottom controls.
     * @param sharedPreferences SharedPreferences instance used to monitor user preference state.
     * @param isNtpWithFakeboxShowingSupplier Supplier telling us if the NTP is showing with a
     *     fakebox. Must have a non-null value immediately available.
     * @param isTabSwitcherFinishedShowingSupplier Supplier indicating whether the tab switcher has
     *     finished showing. It should only reflect `true` once the transition animation has fully
     *     completed.
     * @param isOmniboxFocusedSupplier Supplier of the current omnibox focus state. Must have a
     *     non-null value immediately available.
     * @param isFormFieldFocusedSupplier Supplier of the current form field focus state for the
     *     active WebContents. Must have a non-null value immediately available.
     * @param isFindInPageShowingSupplier Supplier telling us if the "find in page" UI is showing.
     * @param keyboardAccessoryHeightSupplier Supplier of the height of the keyboard accessory,
     *     which stacks on top of the soft keyboard.
     * @param controlContainer The control container for the current context.
     * @param toolbarLayout The layout for toolbar.
     * @param bottomControlsStacker {@link BottomControlsStacker} used to harmonize the position of
     *     the bottom toolbar with other bottom-anchored UI.
     * @param controlContainerHeightSupplier Supplier of an override current height of the control
     *     container. If the value is equal to LayoutParams.WRAP_CONTENT, it should be understood as
     *     meaning that the height should no longer be overridden.
     * @param topInsetCoordinatorSupplier Supplier of the {@link TopInsetCoordinator}.
     * @param controlsPosition Supplier to update whenever toolbar position changes.
     * @param profileSupplier Supplier of the currently applicable profile.
     */
    public ToolbarPositionController(
            BrowserControlsSizer browserControlsSizer,
            SharedPreferences sharedPreferences,
            ObservableSupplier<Boolean> isNtpWithFakeboxShowingSupplier,
            ObservableSupplier<Boolean> isIncognitoNtpShowingSupplier,
            ObservableSupplier<Boolean> isTabSwitcherFinishedShowingSupplier,
            ObservableSupplier<Boolean> isOmniboxFocusedSupplier,
            ObservableSupplier<Boolean> isFormFieldFocusedSupplier,
            ObservableSupplier<Boolean> isFindInPageShowingSupplier,
            ObservableSupplier<Integer> keyboardAccessoryHeightSupplier,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            ControlContainer controlContainer,
            ToolbarLayout toolbarLayout,
            BottomControlsStacker bottomControlsStacker,
            ObservableSupplierImpl<Integer> browserControlsOffsetSupplier,
            View toolbarProgressBarContainer,
            ObservableSupplier<Integer> controlContainerTranslationSupplier,
            ObservableSupplier<Integer> controlContainerHeightSupplier,
            ObservableSupplier<TopInsetCoordinator> topInsetCoordinatorSupplier,
            Handler handler,
            Context context,
            ObservableSupplierImpl<@ControlsPosition Integer> controlsPosition,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Integer> keyboardHeightSupplier,
            WindowAndroid windowAndroid) {
        mBrowserControlsSizer = browserControlsSizer;
        mIsNtpWithFakeboxShowingSupplier = isNtpWithFakeboxShowingSupplier;
        mIsTabSwitcherFinishedShowingSupplier = isTabSwitcherFinishedShowingSupplier;
        mIsOmniboxFocusedSupplier = isOmniboxFocusedSupplier;
        mIsFormFieldFocusedSupplier = isFormFieldFocusedSupplier;
        mIsFindInPageShowingSupplier = isFindInPageShowingSupplier;
        mIsIncognitoNtpShowingSupplier = isIncognitoNtpShowingSupplier;
        mKeyboardAccessoryHeightSupplier = keyboardAccessoryHeightSupplier;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mControlContainer = controlContainer;
        mToolbarLayout = toolbarLayout;
        mBottomControlsStacker = bottomControlsStacker;
        mBrowserControlsOffsetSupplier = browserControlsOffsetSupplier;
        mToolbarProgressBarContainer = toolbarProgressBarContainer;
        mControlContainerTranslationSupplier = controlContainerTranslationSupplier;
        mControlContainerHeightSupplier = controlContainerHeightSupplier;
        mTopInsetCoordinatorSupplier = topInsetCoordinatorSupplier;
        mCurrentPosition = controlsPosition;
        mKeyboardHeightSupplier = keyboardHeightSupplier;
        mWindowAndroid = windowAndroid;
        mCurrentPosition.set(mBrowserControlsSizer.getControlsPosition());
        mProfileSupplier = profileSupplier;

        mHairlineHeight =
                context.getResources().getDimensionPixelSize(R.dimen.toolbar_hairline_height);

        mIsNtpShowingObserver = (showing) -> updateCurrentPosition();
        mIsTabSwitcherFinishedShowingObserver = (showing) -> updateCurrentPosition();
        mIsOmniboxFocusedObserver = (focused) -> updateCurrentPosition();
        mIsFormFieldFocusedObserver =
                (focused) -> updateCurrentPosition(/* prefStateChanged= */ false);
        mIsFindInPageShowingObserver = (showing) -> updateCurrentPosition();
        mKeyboardVisibilityListener =
                (showing) -> updateCurrentPosition(/* prefStateChanged= */ false);

        mIsNtpWithFakeboxShowingSupplier.addObserver(mIsNtpShowingObserver);
        mIsTabSwitcherFinishedShowingSupplier.addObserver(mIsTabSwitcherFinishedShowingObserver);
        mIsOmniboxFocusedSupplier.addObserver(mIsOmniboxFocusedObserver);
        mIsFormFieldFocusedSupplier.addObserver(mIsFormFieldFocusedObserver);
        mIsFindInPageShowingSupplier.addObserver(mIsFindInPageShowingObserver);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        mSharedPreferences = sharedPreferences;
        mSharedPreferences.registerOnSharedPreferenceChangeListener(this);
        recordStartupPosition(isToolbarConfiguredToShowOnTop());

        mLayerVisibility = LayerVisibility.HIDDEN;
        mBottomToolbarLayer =
                new BottomControlsLayerWithOffset() {
                    private int mLayerOffset;

                    @Override
                    public int getLayerOffsetPx() {
                        return mLayerOffset;
                    }

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
                        return mControlContainerHeight;
                    }

                    @Override
                    public int getLayerVisibility() {
                        return mLayerVisibility;
                    }

                    @Override
                    public void onBrowserControlsOffsetUpdate(int layerYOffset) {
                        if (mLayerVisibility == LayerVisibility.VISIBLE) {
                            mLayerOffset = layerYOffset;
                            mBrowserControlsOffsetSupplier.set(layerYOffset);
                            updateViewOffset(this, mControlContainer.getView());
                        }
                    }
                };
        mProgressBarLayer =
                new BottomControlsLayerWithOffset() {
                    private int mLayerOffset;

                    @Override
                    public int getLayerOffsetPx() {
                        return mLayerOffset;
                    }

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
                        if (mLayerVisibility == LayerVisibility.VISIBLE) {
                            mLayerOffset = layerYOffset;
                            updateViewOffset(this, mToolbarProgressBarContainer);
                        }
                    }
                };

        mBottomControlsStacker.addLayer(mBottomToolbarLayer);
        mBottomControlsStacker.addLayer(mProgressBarLayer);

        mKeyboardHeightToolbarCallback =
                (height) -> updateViewOffset(mBottomToolbarLayer, mControlContainer.getView());
        mKeyboardHeightProgressBarCallback =
                (height) -> updateViewOffset(mProgressBarLayer, mToolbarProgressBarContainer);
        mKeyboardVisibilityViewOffsetCallback =
                (showing) -> updateViewOffset(mBottomToolbarLayer, mControlContainer.getView());
        mFormFieldViewOffsetCallback =
                (focused) -> updateViewOffset(mProgressBarLayer, mToolbarProgressBarContainer);
        mIncognitoNtpShowingViewOffsetCallback =
                (showing) -> updateViewOffset(mBottomToolbarLayer, mControlContainer.getView());
        mControlContainerTranslationCallback =
                (offset) -> updateViewOffset(mBottomToolbarLayer, mControlContainer.getView());
        mKeyboardAccessoryHeightObserver =
                (height) -> {
                    if (mCurrentPosition.get() == ControlsPosition.TOP) {
                        mControlContainer.mutateLayoutParams().bottomMargin = 0;
                        return;
                    }
                    mControlContainer.mutateLayoutParams().bottomMargin = height;
                };
        mControlContainerHeightCallback = this::updateControlContainerHeight;
        mControlContainerHeightSupplier.addSyncObserverAndCallIfNonNull(
                mControlContainerHeightCallback);

        mKeyboardAccessoryHeightSupplier.addObserver(mKeyboardAccessoryHeightObserver);
        mKeyboardAccessoryHeightSupplier.addObserver(mKeyboardHeightProgressBarCallback);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(
                mKeyboardVisibilityViewOffsetCallback);
        mIsFormFieldFocusedSupplier.addObserver(mFormFieldViewOffsetCallback);
        mIsIncognitoNtpShowingSupplier.addObserver(mIncognitoNtpShowingViewOffsetCallback);
        mControlContainerTranslationSupplier.addObserver(mControlContainerTranslationCallback);
        mKeyboardHeightSupplier.addObserver(mKeyboardHeightToolbarCallback);
        mKeyboardHeightSupplier.addObserver(mKeyboardHeightProgressBarCallback);

        var topInsetCoordinator = mTopInsetCoordinatorSupplier.get();
        if (topInsetCoordinator != null) {
            onTopInsetCoordinatorAvailable(topInsetCoordinator);
        } else {
            mTopInsetCoordinatorAvailableCallback = this::onTopInsetCoordinatorAvailable;
            mTopInsetCoordinatorSupplier.addObserver(mTopInsetCoordinatorAvailableCallback);
        }

        updateCurrentPosition();
        mHandler = handler;
    }

    public void destroy() {
        mIsNtpWithFakeboxShowingSupplier.removeObserver(mIsNtpShowingObserver);
        mIsTabSwitcherFinishedShowingSupplier.removeObserver(mIsTabSwitcherFinishedShowingObserver);
        mIsOmniboxFocusedSupplier.removeObserver(mIsOmniboxFocusedObserver);
        mIsFormFieldFocusedSupplier.removeObserver(mIsFormFieldFocusedObserver);
        mIsFindInPageShowingSupplier.removeObserver(mIsFindInPageShowingObserver);
        mIsIncognitoNtpShowingSupplier.removeObserver(mIncognitoNtpShowingViewOffsetCallback);
        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        mSharedPreferences.unregisterOnSharedPreferenceChangeListener(this);
        mKeyboardAccessoryHeightSupplier.removeObserver(mKeyboardHeightToolbarCallback);
        mKeyboardAccessoryHeightSupplier.removeObserver(mKeyboardHeightProgressBarCallback);
        mKeyboardHeightSupplier.removeObserver(mKeyboardHeightToolbarCallback);
        mKeyboardHeightSupplier.removeObserver(mKeyboardHeightProgressBarCallback);
        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(
                mKeyboardVisibilityViewOffsetCallback);
        mIsFormFieldFocusedSupplier.removeObserver(mFormFieldViewOffsetCallback);
        mControlContainerTranslationSupplier.removeObserver(mControlContainerTranslationCallback);
        mControlContainerHeightSupplier.removeObserver(mControlContainerHeightCallback);
        mKeyboardAccessoryHeightSupplier.removeObserver(mKeyboardAccessoryHeightObserver);
        if (mTopInsetCoordinatorObserver != null) {
            var topInsetCoordinator = mTopInsetCoordinatorSupplier.get();
            if (topInsetCoordinator != null) {
                topInsetCoordinator.removeObserver(mTopInsetCoordinatorObserver);
            }
            mTopInsetCoordinatorObserver = null;
        }
        if (mTopInsetCoordinatorAvailableCallback != null) {
            mTopInsetCoordinatorSupplier.removeObserver(mTopInsetCoordinatorAvailableCallback);
            mTopInsetCoordinatorAvailableCallback = null;
        }
    }

    /**
     * Whether the current position matches the user-configured one, e.g. if the configured position
     * is bottom but the omnibox is focused.
     */
    public boolean doesPrefMismatchPosition() {
        @ControlsPosition
        int positionForPref =
                isToolbarConfiguredToShowOnTop() ? ControlsPosition.TOP : ControlsPosition.BOTTOM;
        return assumeNonNull(mCurrentPosition.get()) != positionForPref;
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
                && !DeviceInfo.isAutomotive()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    @Override
    public void onSharedPreferenceChanged(
            SharedPreferences sharedPreferences, @Nullable String key) {
        if (ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED.equals(key)) {
            // Re-set placement to retrieve it from prefs upon next access.
            sToolbarShouldShowOnTop = null;
            recordPrefChange(isToolbarConfiguredToShowOnTop());
            updateCurrentPosition(/* prefStateChanged= */ true);
        }
    }

    /** Returns true if toolbar is user-configured to show on top. */
    private static boolean isToolbarConfiguredToShowOnTop() {
        if (sToolbarShouldShowOnTop == null) {
            sToolbarShouldShowOnTop = AddressBarPreference.isToolbarConfiguredToShowOnTop();
        }
        return sToolbarShouldShowOnTop;
    }

    private void updateCurrentPosition() {
        updateCurrentPosition(/* prefStateChanged= */ false);
    }

    private void updateCurrentPosition(boolean prefStateChanged) {
        boolean ntpShowing = mIsNtpWithFakeboxShowingSupplier.get();
        boolean tabSwitcherShowing = mIsTabSwitcherFinishedShowingSupplier.get();
        boolean isOmniboxFocused = mIsOmniboxFocusedSupplier.get();
        boolean isFindInPageShowing = mIsFindInPageShowingSupplier.get();
        boolean isFormFieldFocusedWithKeyboardVisible =
                mIsFormFieldFocusedSupplier.get()
                        && mKeyboardVisibilityDelegate.isKeyboardShowing(
                                mControlContainer.getView());
        @StateTransition
        int stateTransition =
                calculateStateTransition(
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        isToolbarConfiguredToShowOnTop(),
                        assumeNonNull(mCurrentPosition.get()));
        @ControlsPosition
        int newControlsPosition =
                switch (stateTransition) {
                    case StateTransition.SNAP_TO_BOTTOM, StateTransition.ANIMATE_TO_BOTTOM ->
                            ControlsPosition.BOTTOM;
                    case StateTransition.SNAP_TO_TOP, StateTransition.ANIMATE_TO_TOP ->
                            ControlsPosition.TOP;
                    default -> mCurrentPosition.get();
                };

        if (newControlsPosition == mCurrentPosition.get()) return;

        int newTopHeight;
        int controlContainerHeight = mControlContainer.getToolbarHeight();
        mCurrentPosition.set(newControlsPosition);

        if (newControlsPosition == ControlsPosition.TOP) {
            newTopHeight = mBrowserControlsSizer.getTopControlsHeight() + controlContainerHeight;
            mLayerVisibility = LayerVisibility.HIDDEN;
            mControlContainer.getView().setTranslationY(0);
            mToolbarProgressBarContainer.setTranslationY(0);
            Runnable progressBarChangeRunnable =
                    () -> {
                        // Bail out if there was a state change while we waited for the runnable to
                        // execute.
                        if (mCurrentPosition.get() != ControlsPosition.TOP) return;
                        LayoutParams progressBarLayoutParams =
                                (LayoutParams) mToolbarProgressBarContainer.getLayoutParams();
                        progressBarLayoutParams.setAnchorId(mControlContainer.getView().getId());
                        progressBarLayoutParams.anchorGravity = Gravity.BOTTOM;
                        if (ChromeFeatureList.sAndroidAnimatedProgressBarInBrowser.isEnabled()) {
                            progressBarLayoutParams.gravity = Gravity.BOTTOM;
                        } else {
                            progressBarLayoutParams.gravity = Gravity.CENTER;
                        }
                        mToolbarProgressBarContainer.setLayoutParams(progressBarLayoutParams);
                    };

            if (((ViewGroup) mToolbarProgressBarContainer.getParent()).isInLayout()) {
                mHandler.post(progressBarChangeRunnable);
            } else {
                progressBarChangeRunnable.run();
            }
        } else {
            newTopHeight = mBrowserControlsSizer.getTopControlsHeight() - controlContainerHeight;
            mLayerVisibility = LayerVisibility.VISIBLE;
            CoordinatorLayout.LayoutParams progressBarLayoutParams =
                    (LayoutParams) mToolbarProgressBarContainer.getLayoutParams();
            progressBarLayoutParams.setAnchorId(View.NO_ID);
            progressBarLayoutParams.anchorGravity = Gravity.NO_GRAVITY;
            progressBarLayoutParams.gravity = Gravity.BOTTOM;
            mToolbarProgressBarContainer.setLayoutParams(progressBarLayoutParams);
        }

        boolean animatingToTop = stateTransition == StateTransition.ANIMATE_TO_TOP;
        boolean animatingToBottom = stateTransition == StateTransition.ANIMATE_TO_BOTTOM;

        mBottomControlsStacker.updateLayerVisibilitiesAndSizes();
        if (animatingToTop || animatingToBottom) {
            mBrowserControlsSizer.setAnimateBrowserControlsHeightChanges(true);
            // Prevent a visual glitch when animating the control container into a new location by
            // making it immediately invisible. Without this, it can show for a single frame before
            // hiding then sliding into place.
            mControlContainer.getView().setVisibility(View.INVISIBLE);
        }

        mBrowserControlsSizer.setControlsPosition(
                newControlsPosition,
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
        CoordinatorLayout.LayoutParams hairlineLayoutParams =
                mControlContainer.mutateHairlineLayoutParams();
        hairlineLayoutParams.anchorGravity =
                newControlsPosition == ControlsPosition.TOP ? Gravity.BOTTOM : Gravity.TOP;
        hairlineLayoutParams.gravity = hairlineLayoutParams.anchorGravity;
        LayoutParams layoutParams = mControlContainer.mutateLayoutParams();
        int verticalGravity =
                newControlsPosition == ControlsPosition.TOP ? Gravity.TOP : Gravity.BOTTOM;
        layoutParams.gravity = Gravity.START | verticalGravity;
        CoordinatorLayout.LayoutParams toolbarLayoutParams =
                mControlContainer.mutateToolbarLayoutParams();
        toolbarLayoutParams.topMargin =
                newControlsPosition == ControlsPosition.BOTTOM ? mHairlineHeight : 0;
        toolbarLayoutParams.bottomMargin =
                newControlsPosition == ControlsPosition.BOTTOM ? 0 : mHairlineHeight;

        // Set that the bottom omnibox has been used at least once now.
        if (newControlsPosition == ControlsPosition.BOTTOM && mProfileSupplier.get() != null) {
            UserPrefs.get(mProfileSupplier.get()).setBoolean(BOTTOM_OMNIBOX_EVER_USED_PREF, true);
        }
    }

    @VisibleForTesting
    static @StateTransition int calculateStateTransition(
            boolean prefStateChanged,
            boolean ntpShowing,
            boolean tabSwitcherShowing,
            boolean isOmniboxFocused,
            boolean isFindInPageShowing,
            boolean isFormFieldFocusedWithKeyboardVisible,
            boolean doesUserPreferTopToolbar,
            @ControlsPosition int currentPosition) {
        boolean miniOriginBarEnabled = ChromeFeatureList.sMiniOriginBar.isEnabled();
        boolean allowBottomAnchoredFocusedOmnibox =
                ChromeFeatureList.sAndroidBottomToolbarV2.isEnabled();
        boolean forceBottomForFocusedOmnibox =
                isOmniboxFocused
                        && (ChromeFeatureList.sAndroidBottomToolbarV2ForceBottomForFocusedOmnibox
                                        .getValue()
                                || (allowBottomAnchoredFocusedOmnibox
                                        && !doesUserPreferTopToolbar));
        @ControlsPosition int newControlsPosition;
        if (!forceBottomForFocusedOmnibox
                && (ntpShowing
                        || tabSwitcherShowing
                        || (isOmniboxFocused && !allowBottomAnchoredFocusedOmnibox)
                        || isFindInPageShowing
                        || (isFormFieldFocusedWithKeyboardVisible && !miniOriginBarEnabled)
                        || doesUserPreferTopToolbar)) {
            newControlsPosition = ControlsPosition.TOP;
        } else {
            newControlsPosition = ControlsPosition.BOTTOM;
        }

        boolean switchingToBottom = newControlsPosition == ControlsPosition.BOTTOM;
        if (newControlsPosition == currentPosition) {
            // Don't do anything for non-transitions.
            return StateTransition.NONE;
        } else if (prefStateChanged) {
            // Animate when the pref changes via the long press menu, but not if it was changed via
            // the settings UI.
            int positionAndSource = computeToolbarPositionAndSource();
            boolean animate =
                    positionAndSource == ToolbarPositionAndSource.TOP_LONG_PRESS
                            || positionAndSource == ToolbarPositionAndSource.BOTTOM_LONG_PRESS;
            if (animate) {
                return switchingToBottom
                        ? StateTransition.ANIMATE_TO_BOTTOM
                        : StateTransition.ANIMATE_TO_TOP;
            } else {
                return switchingToBottom
                        ? StateTransition.SNAP_TO_BOTTOM
                        : StateTransition.SNAP_TO_TOP;
            }
        }

        // For all other state transitions, just snap to the correct position immediately.
        return switchingToBottom ? StateTransition.SNAP_TO_BOTTOM : StateTransition.SNAP_TO_TOP;
    }

    private void updateViewOffset(BottomControlsLayerWithOffset layer, View viewForLayer) {
        if (mLayerVisibility != LayerVisibility.VISIBLE) return;

        int layerYOffset = layer.getLayerOffsetPx() + mControlContainerTranslationSupplier.get();
        int chinHeight =
                mBottomControlsStacker.isLayerVisible(LayerType.BOTTOM_CHIN)
                        ? mBottomControlsStacker.getHeightFromLayerToBottom(LayerType.BOTTOM_CHIN)
                        : 0;
        // The chin overlaps with the accessory when they're both visible. To avoid double counting,
        // remove the chin's height from the final offset.
        boolean chinVisibleWithAccessory =
                mKeyboardAccessoryHeightSupplier.get() > 0 && chinHeight > 0;
        if (chinVisibleWithAccessory) {
            layerYOffset += chinHeight;
        }

        if (mIsOmniboxFocusedSupplier.get()
                && assumeNonNull(mCurrentPosition.get()) == ControlsPosition.BOTTOM) {
            WindowInsetsCompat windowInsetsCompat =
                    WindowInsetsCompat.toWindowInsetsCompat(
                            mControlContainer.getView().getRootWindowInsets(),
                            mControlContainer.getView().getRootView());

            int keyboardHeight = windowInsetsCompat.getInsets(WindowInsetsCompat.Type.ime()).bottom;

            if (shouldIgnoreKeyboardHeightForIncognitoNtp()) {
                keyboardHeight = 0;
            }

            int statusBarHeight =
                    windowInsetsCompat.getInsets(WindowInsetsCompat.Type.statusBars()).top;
            // The control container can grow quite large with a multiline url bar, making its full
            // height unrenderable in the amount of space available between the keyboard and window
            // top. We restrict its position and height to allow scrolling and avoid rendering
            // offscreen.
            int windowHeight = mWindowAndroid.getDisplay().getDisplayHeight();
            int maxHeight = windowHeight - keyboardHeight - statusBarHeight;
            mControlContainer.setMaxHeight(maxHeight);

            int maxTranslation = -(windowHeight - layer.getHeight() - statusBarHeight);
            // The translation is negative so we take the arithmetic max to get the minimum visible
            // delta.
            layerYOffset = Math.max(layerYOffset - keyboardHeight, maxTranslation);
        } else {
            mControlContainer.setMaxHeight(Integer.MAX_VALUE);
        }

        viewForLayer.setTranslationY(layerYOffset);
        if (layer == mBottomToolbarLayer) {
            mBrowserControlsOffsetSupplier.set(layerYOffset);
        }
    }

    private void updateControlContainerHeight(int height) {
        assert height >= 0;
        mControlContainerHeight = height;
        mBottomControlsStacker.requestLayerUpdate(false);
    }

    /**
     * Returns whether the keyboard height should be ignored for toolbar's Y offset calculation when
     * omnibox is focused. Can return {@code true} only when {@code AndroidBottomToolbarV2} and
     * {@code OmniboxAutofocusOnIncognitoNtp} features are enabled.
     *
     * <p>{@code OmniboxAutofocusOnIncognitoNtp} feature requires the keyboard to not be in overlay
     * mode to work correctly. This is managed by not attaching the {@code
     * DeferredIMEWindowInsetApplicationCallback} in {@code AutocompleteMediator}.
     *
     * @return Whether the keyboard height should be ignored.
     */
    private boolean shouldIgnoreKeyboardHeightForIncognitoNtp() {
        InsetObserver insetObserver = mWindowAndroid.getInsetObserver();
        // If the inset observer isn't available but the Incognito NTP Omnibox Autofocus is
        // active, we assume the keyboard is in resizing mode.
        boolean isKeyboardInResizingMode =
                insetObserver == null || !insetObserver.isKeyboardInOverlayMode();

        boolean isIncognitoNtpShowing = mIsIncognitoNtpShowingSupplier.get();
        boolean isOmniboxFocused = mIsOmniboxFocusedSupplier.get();

        boolean allowOmniboxAutofocusOnIncognitoNtp =
                ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtp.isEnabled();
        boolean allowForceBottomForFocusedOmnibox =
                ChromeFeatureList.sAndroidBottomToolbarV2ForceBottomForFocusedOmnibox.getValue()
                        || (ChromeFeatureList.sAndroidBottomToolbarV2.isEnabled()
                                && !isToolbarConfiguredToShowOnTop());

        return allowForceBottomForFocusedOmnibox
                && allowOmniboxAutofocusOnIncognitoNtp
                && isIncognitoNtpShowing
                && isOmniboxFocused
                && isKeyboardInResizingMode;
    }

    /** Returns whether the toolbar will be shown on top for the supplied tab. */
    public static boolean shouldShowToolbarOnTop(@Nullable Tab tab) {
        // TODO(https://g-issues.chromium.org/issues/420271795): consider fakebox presence here.
        boolean isRegularNtp =
                (tab != null)
                        && (tab.getUrl() != null)
                        && UrlUtilities.isNtpUrl(tab.getUrl())
                        && !tab.isIncognitoBranded();

        return calculateStateTransition(
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

    private void onTopInsetCoordinatorAvailable(TopInsetCoordinator topInsetCoordinator) {
        mTopInsetCoordinatorObserver = this::onToEdgeChange;
        topInsetCoordinator.addObserver(mTopInsetCoordinatorObserver);

        if (mTopInsetCoordinatorAvailableCallback != null) {
            mTopInsetCoordinatorSupplier.removeObserver(mTopInsetCoordinatorAvailableCallback);
            mTopInsetCoordinatorAvailableCallback = null;
        }
    }

    /**
     * Called when the toolbar's embedder surface layout changes between edge-to-edge and standard.
     *
     * @param systemTopInset The system's top inset, i.e., the height of the Status bar. It is
     *     always bigger than 0.
     * @param consumeTopInset Determines if the toolbar should utilize this top inset, extending
     *     across the full height of both the status bar and itself.
     */
    @VisibleForTesting
    void onToEdgeChange(int systemTopInset, boolean consumeTopInset) {
        // Exits early if the top padding doesn't need adjusting.
        if (NtpCustomizationUtils.shouldSkipTopInsetsChange(
                mTopInset, systemTopInset, consumeTopInset)) {
            return;
        }

        mTopInset = consumeTopInset ? systemTopInset : 0;
        mToolbarLayout.onToEdgeChange(mTopInset);
    }
}
