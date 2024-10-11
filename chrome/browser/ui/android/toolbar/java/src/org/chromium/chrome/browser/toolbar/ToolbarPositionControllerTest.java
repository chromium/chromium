// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.pm.PackageManager;
import android.view.Gravity;

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
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;

/** Unit tests for {@link ToolbarPositionController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ToolbarPositionControllerTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    private static final int TOOLBAR_HEIGHT = 56;

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
    @Mock private ControlContainer mControlContainer;

    private Context mContext;
    private ObservableSupplierImpl<Boolean> mIsNtpShowing = new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<Boolean> mIsOmniboxFocused = new ObservableSupplierImpl<>();

    private ToolbarPositionController mController;

    @Before
    public void setUp() {
        doReturn(TOOLBAR_HEIGHT).when(mControlContainer).getToolbarHeight();
        doReturn(mControlContainerLayoutParams).when(mControlContainer).mutateLayoutParams();
        mContext = ContextUtils.getApplicationContext();
        mBrowserControlsSizer.setControlsPosition(ControlsPosition.TOP, TOOLBAR_HEIGHT, 0, 0, 0);
        mControlContainerLayoutParams.gravity = Gravity.START | Gravity.TOP;
        mIsNtpShowing.set(false);
        mIsOmniboxFocused.set(false);
        mController =
                new ToolbarPositionController(
                        mBrowserControlsSizer,
                        ContextUtils.getAppSharedPreferences(),
                        mIsNtpShowing,
                        mIsOmniboxFocused,
                        mControlContainer);
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

    private void assertControlsAtBottom() {
        assertEquals(mBrowserControlsSizer.getControlsPosition(), ControlsPosition.BOTTOM);
        assertEquals(mBrowserControlsSizer.getTopControlsHeight(), 0);
        assertEquals(mBrowserControlsSizer.getBottomControlsHeight(), TOOLBAR_HEIGHT);
        assertEquals(mControlContainerLayoutParams.gravity, Gravity.START | Gravity.BOTTOM);
    }

    private void assertControlsAtTop() {
        assertEquals(mBrowserControlsSizer.getControlsPosition(), ControlsPosition.TOP);
        assertEquals(mBrowserControlsSizer.getTopControlsHeight(), TOOLBAR_HEIGHT);
        assertEquals(mBrowserControlsSizer.getBottomControlsHeight(), 0);
        assertEquals(mControlContainerLayoutParams.gravity, Gravity.START | Gravity.TOP);
    }
}
