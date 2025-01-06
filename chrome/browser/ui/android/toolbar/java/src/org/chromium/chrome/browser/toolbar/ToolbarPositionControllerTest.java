// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.pm.PackageManager;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.util.Observer;

/** Unit tests for {@link ToolbarPositionController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ToolbarPositionControllerTest {

    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    private static final int TOOLBAR_HEIGHT = 56;
    private static final int CONTROL_CONTAINER_ID = 12356;

    private BrowserControlsSizer mBrowserControlsSizer =
            new BrowserControlsSizer() {
                @ControlsPosition private int mControlsPosition = ControlsPosition.TOP;
                private int mTopControlsHeight;
                private int mTopControlsMinHeight;
                private int mBottomControlsHeight;
                private int mBottomControlsMinHeight;

                @Override
                public void setBottomControlsHeight(
                        int bottomControlsHeight, int bottomControlsMinHeight) {
                    mBottomControlsHeight = bottomControlsHeight;
                    mBottomControlsMinHeight = bottomControlsMinHeight;
                }

                @Override
                public void setTopControlsHeight(int topControlsHeight, int topControlsMinHeight) {
                    mTopControlsHeight = topControlsHeight;
                    mTopControlsMinHeight = topControlsMinHeight;
                }

                @Override
                public void setAnimateBrowserControlsHeightChanges(
                        boolean animateBrowserControlsHeightChanges) {}

                @Override
                public void notifyBackgroundColor(int color) {}

                @Override
                public void setControlsPosition(
                        int controlsPosition,
                        int newTopControlsHeight,
                        int newTopControlsMinHeight,
                        int newBottomControlsHeight,
                        int newBottomControlsMinHeight) {
                    mControlsPosition = controlsPosition;
                    setTopControlsHeight(newTopControlsHeight, newTopControlsMinHeight);
                    setBottomControlsHeight(newBottomControlsHeight, newBottomControlsMinHeight);
                }

                @Override
                public BrowserStateBrowserControlsVisibilityDelegate
                        getBrowserVisibilityDelegate() {
                    return null;
                }

                @Override
                public void showAndroidControls(boolean animate) {}

                @Override
                public void restoreControlsPositions() {}

                @Override
                public boolean offsetOverridden() {
                    return false;
                }

                @Override
                public int hideAndroidControlsAndClearOldToken(int oldToken) {
                    return 0;
                }

                @Override
                public void releaseAndroidControlsHidingToken(int token) {}

                @Override
                public void addObserver(Observer obs) {}

                @Override
                public void removeObserver(Observer obs) {}

                @Override
                public int getTopControlsHeight() {
                    return mTopControlsHeight;
                }

                @Override
                public int getTopControlsMinHeight() {
                    return mTopControlsMinHeight;
                }

                @Override
                public int getTopControlOffset() {
                    return 0;
                }

                @Override
                public int getTopControlsMinHeightOffset() {
                    return 0;
                }

                @Override
                public int getBottomControlsHeight() {
                    return mBottomControlsHeight;
                }

                @Override
                public int getBottomControlsMinHeight() {
                    return mBottomControlsMinHeight;
                }

                @Override
                public int getBottomControlsMinHeightOffset() {
                    return 0;
                }

                @Override
                public boolean shouldAnimateBrowserControlsHeightChanges() {
                    return false;
                }

                @Override
                public boolean shouldUpdateOffsetsWhenConstraintsChange() {
                    return false;
                }

                @Override
                public int getBottomControlOffset() {
                    return 0;
                }

                @Override
                public float getBrowserControlHiddenRatio() {
                    return 0;
                }

                @Override
                public int getContentOffset() {
                    return 0;
                }

                @Override
                public float getTopVisibleContentOffset() {
                    return 0;
                }

                @Override
                public int getAndroidControlsVisibility() {
                    return 0;
                }

                @Override
                public int getControlsPosition() {
                    return mControlsPosition;
                }
            };

    private CoordinatorLayout.LayoutParams mControlContainerLayoutParams =
            new CoordinatorLayout.LayoutParams(400, TOOLBAR_HEIGHT);
    private CoordinatorLayout.LayoutParams mProgressBarLayoutParams =
            new CoordinatorLayout.LayoutParams(400, 5);
    private FrameLayout.LayoutParams mHairlineLayoutParams = new LayoutParams(400, 5);
    @Mock private ControlContainer mControlContainer;
    @Mock private View mControlContainerView;
    @Mock private View mProgressBarContainer;

    private Context mContext;
    private ObservableSupplierImpl<Boolean> mIsNtpShowing = new ObservableSupplierImpl<>(false);
    private ObservableSupplierImpl<Boolean> mIsTabSwitcherShowing = new ObservableSupplierImpl<>(false);
    private ObservableSupplierImpl<Boolean> mIsOmniboxFocused = new ObservableSupplierImpl<>(false);
    private ObservableSupplierImpl<Boolean> mIsFindInPageShowing =
            new ObservableSupplierImpl<>(false);
    private FormFieldFocusedSupplier mIsFormFieldFocused = new FormFieldFocusedSupplier();
    private BottomControlsStacker mBottomControlsStacker;
    private ToolbarPositionController mController;
    private ObservableSupplierImpl<Integer> mBottomToolbarOffsetSupplier =
            new ObservableSupplierImpl<>();

    static class FakeKeyboardVisibilityDelegate extends KeyboardVisibilityDelegate {
        private boolean mIsShowing;

        public void setVisibilityForTests(boolean isShowing) {
            mIsShowing = isShowing;
            notifyListeners(isShowing);
        }

        @Override
        public boolean isKeyboardShowing(Context context, View view) {
            return mIsShowing;
        }
    }
    ;

    private FakeKeyboardVisibilityDelegate mKeyboardVisibilityDelegate =
            new FakeKeyboardVisibilityDelegate();

    @Before
    public void setUp() {
        doReturn(TOOLBAR_HEIGHT).when(mControlContainer).getToolbarHeight();
        doReturn(mControlContainerLayoutParams).when(mControlContainer).mutateLayoutParams();
        mHairlineLayoutParams.topMargin = TOOLBAR_HEIGHT;
        doReturn(mHairlineLayoutParams).when(mControlContainer).mutateHairlineLayoutParams();
        doReturn(mControlContainerView).when(mControlContainer).getView();
        doReturn(CONTROL_CONTAINER_ID).when(mControlContainerView).getId();
        doReturn(mProgressBarLayoutParams).when(mProgressBarContainer).getLayoutParams();
        mContext = ContextUtils.getApplicationContext();
        doReturn(mContext.getResources()).when(mProgressBarContainer).getResources();
        mBottomControlsStacker = new BottomControlsStacker(mBrowserControlsSizer);
        mBrowserControlsSizer.setControlsPosition(ControlsPosition.TOP, TOOLBAR_HEIGHT, 0, 0, 0);
        mControlContainerLayoutParams.gravity = Gravity.START | Gravity.TOP;
        mProgressBarLayoutParams.gravity = Gravity.TOP;
        mProgressBarLayoutParams.anchorGravity = Gravity.BOTTOM;
        mProgressBarLayoutParams.setAnchorId(CONTROL_CONTAINER_ID);
        mController =
                new ToolbarPositionController(
                        mBrowserControlsSizer,
                        ContextUtils.getAppSharedPreferences(),
                        mIsNtpShowing,
                        mIsTabSwitcherShowing,
                        mIsOmniboxFocused,
                        mIsFormFieldFocused,
                        mIsFindInPageShowing,
                        mKeyboardVisibilityDelegate,
                        mControlContainer,
                        mBottomControlsStacker,
                        mBottomToolbarOffsetSupplier,
                        mProgressBarContainer,
                        mContext);
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testIsToolbarPositionCustomizationEnabled_tablet() {
        assertFalse(
                ToolbarPositionController.isToolbarPositionCustomizationEnabled(mContext, false));
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testIsToolbarPositionCustomizationEnabled_phone() {
        assertFalse(
                ToolbarPositionController.isToolbarPositionCustomizationEnabled(mContext, true));
        assertTrue(
                ToolbarPositionController.isToolbarPositionCustomizationEnabled(mContext, false));
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testIsToolbarPositionCustomizationEnabled_featureDisabled() {
        assertFalse(
                ToolbarPositionController.isToolbarPositionCustomizationEnabled(mContext, true));
        assertFalse(
                ToolbarPositionController.isToolbarPositionCustomizationEnabled(mContext, false));
    }

    @Test
    @Config(qualifiers = "sw400dp", minSdk = android.os.Build.VERSION_CODES.R)
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testIsToolbarPositionCustomizationEnabled_foldable() {
        ShadowPackageManager shadowPackageManager = Shadows.shadowOf(mContext.getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE, true);
        // Foldable check is disabled on debug builds to work around emulators reporting as
        // foldable.
        if (!BuildInfo.isDebugApp()) {
            assertFalse(
                    ToolbarPositionController.isToolbarPositionCustomizationEnabled(
                            mContext, false));
        }
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testUpdatePositionChangesWithPref() {
        assertControlsAtTop();
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false)
                .commit();
        assertControlsAtBottom();

        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true)
                .commit();
        assertControlsAtTop();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testUpdatePositionChangesWithNtpState() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false)
                .commit();
        assertControlsAtBottom();

        mIsNtpShowing.set(true);
        assertControlsAtTop();

        mIsNtpShowing.set(false);
        assertControlsAtBottom();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testUpdatePositionChangesWithTabSwitcherState() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false)
                .commit();
        assertControlsAtBottom();

        mIsTabSwitcherShowing.set(true);
        assertControlsAtTop();

        mIsTabSwitcherShowing.set(false);
        assertControlsAtBottom();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testUpdatePositionChangesWithOmniboxFocusState() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false)
                .commit();
        assertControlsAtBottom();

        mIsOmniboxFocused.set(true);
        assertControlsAtTop();

        mIsOmniboxFocused.set(false);
        assertControlsAtBottom();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testUpdatePositionChangesWithFormFieldFocusState() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false)
                .commit();
        assertControlsAtBottom();

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        assertControlsAtTop();

        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        assertControlsAtBottom();

        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        assertControlsAtTop();

        mIsFormFieldFocused.onNodeAttributeUpdated(false, false);
        assertControlsAtBottom();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testUpdatePositionChangesWithFindInPage() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false)
                .commit();
        assertControlsAtBottom();

        mIsFindInPageShowing.set(true);
        assertControlsAtTop();

        mIsFindInPageShowing.set(false);
        assertControlsAtBottom();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testBottomControlsStacker() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false)
                .commit();
        assertControlsAtBottom();

        assertEquals(mBottomControlsStacker.getTotalHeight(), TOOLBAR_HEIGHT);
        BottomControlsLayer toolbarLayer =
                mBottomControlsStacker.getLayerForTesting(LayerType.BOTTOM_TOOLBAR);
        assertEquals(toolbarLayer.getHeight(), TOOLBAR_HEIGHT);
        assertEquals(toolbarLayer.getLayerVisibility(), LayerVisibility.VISIBLE);
        assertEquals(toolbarLayer.getScrollBehavior(), LayerScrollBehavior.DEFAULT_SCROLL_OFF);

        toolbarLayer.onBrowserControlsOffsetUpdate(12);
        verify(mControlContainerView).setTranslationY(12);
        assertEquals(mBottomToolbarOffsetSupplier.get().intValue(), 12);

        BottomControlsLayer progressBarLayer =
                mBottomControlsStacker.getLayerForTesting(LayerType.PROGRESS_BAR);
        assertEquals(progressBarLayer.getHeight(), 0);
        assertEquals(progressBarLayer.getLayerVisibility(), LayerVisibility.VISIBLE);
        assertEquals(progressBarLayer.getScrollBehavior(), LayerScrollBehavior.DEFAULT_SCROLL_OFF);

        progressBarLayer.onBrowserControlsOffsetUpdate(-12);
        verify(mProgressBarContainer).setTranslationY(-12);

        mIsOmniboxFocused.set(true);
        assertControlsAtTop();
        assertEquals(toolbarLayer.getLayerVisibility(), LayerVisibility.HIDDEN);
        verify(mControlContainerView).setTranslationY(0);
        assertEquals(progressBarLayer.getLayerVisibility(), LayerVisibility.HIDDEN);
        verify(mProgressBarContainer).setTranslationY(0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testGetToolbarPositionResId() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true)
                .commit();
        assertEquals(
                R.string.address_bar_settings_top,
                ToolbarPositionController.getToolbarPositionResId());

        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, false)
                .commit();
        assertEquals(
                R.string.address_bar_settings_bottom,
                ToolbarPositionController.getToolbarPositionResId());
    }

    private void assertControlsAtBottom() {
        assertEquals(mBrowserControlsSizer.getControlsPosition(), ControlsPosition.BOTTOM);
        assertEquals(mBrowserControlsSizer.getTopControlsHeight(), 0);
        assertEquals(mBrowserControlsSizer.getBottomControlsHeight(), TOOLBAR_HEIGHT);
        assertEquals(mHairlineLayoutParams.topMargin, 0);
        assertEquals(mControlContainerLayoutParams.gravity, Gravity.START | Gravity.BOTTOM);
        assertEquals(mProgressBarLayoutParams.gravity, Gravity.BOTTOM);
        assertEquals(mProgressBarLayoutParams.anchorGravity, Gravity.NO_GRAVITY);
        assertEquals(mProgressBarLayoutParams.getAnchorId(), View.NO_ID);
    }

    private void assertControlsAtTop() {
        assertEquals(mBrowserControlsSizer.getControlsPosition(), ControlsPosition.TOP);
        assertEquals(mBrowserControlsSizer.getTopControlsHeight(), TOOLBAR_HEIGHT);
        assertEquals(mBrowserControlsSizer.getBottomControlsHeight(), 0);
        assertEquals(mHairlineLayoutParams.topMargin, TOOLBAR_HEIGHT);
        assertEquals(mControlContainerLayoutParams.gravity, Gravity.START | Gravity.TOP);
        assertEquals(mProgressBarLayoutParams.gravity, Gravity.TOP);
        assertEquals(mProgressBarLayoutParams.anchorGravity, Gravity.BOTTOM);
        assertEquals(mProgressBarLayoutParams.getAnchorId(), CONTROL_CONTAINER_ID);
    }
}
