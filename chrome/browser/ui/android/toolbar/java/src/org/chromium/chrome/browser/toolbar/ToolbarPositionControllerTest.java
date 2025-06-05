// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.BottomControlsLayerWithOffset;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.StateTransition;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Unit tests for {@link ToolbarPositionController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ToolbarPositionControllerTest {

    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    private static final int TOOLBAR_HEIGHT = 56;
    private static final int CONTROL_CONTAINER_ID = 12356;

    private final BrowserControlsSizer mBrowserControlsSizer =
            new BrowserControlsSizer() {
                @ControlsPosition private int mControlsPosition = ControlsPosition.TOP;
                private int mTopControlsHeight;
                private int mTopControlsMinHeight;
                private int mRendererTopControlsOffset;
                private int mBottomControlsHeight;
                private int mBottomControlsMinHeight;
                private int mRendererBottomControlsOffset;

                @Override
                public void setBottomControlsHeight(
                        int bottomControlsHeight, int bottomControlsMinHeight) {
                    mBottomControlsHeight = bottomControlsHeight;
                    mBottomControlsMinHeight = bottomControlsMinHeight;
                }

                @Override
                public void setBottomControlsAdditionalHeight(int height) {}

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
                        @ControlsPosition int controlsPosition,
                        int newTopControlsHeight,
                        int newTopControlsMinHeight,
                        int newRendererTopControlsOffset,
                        int newBottomControlsHeight,
                        int newBottomControlsMinHeight,
                        int newRendererBottomControlsOffset) {
                    mControlsPosition = controlsPosition;
                    mRendererTopControlsOffset = newRendererTopControlsOffset;
                    mRendererBottomControlsOffset = newRendererBottomControlsOffset;
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
                public int getTopControlsHairlineHeight() {
                    return 0;
                }

                @Override
                public int getTopControlsMinHeight() {
                    return mTopControlsMinHeight;
                }

                @Override
                public int getTopControlOffset() {
                    return mRendererTopControlsOffset;
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
                    return mRendererBottomControlsOffset;
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

                @Override
                public boolean isVisibilityForced() {
                    return false;
                }
            };

    private final CoordinatorLayout.LayoutParams mControlContainerLayoutParams =
            new CoordinatorLayout.LayoutParams(400, TOOLBAR_HEIGHT);
    private final CoordinatorLayout.LayoutParams mProgressBarLayoutParams =
            new CoordinatorLayout.LayoutParams(400, 5);
    private final FrameLayout.LayoutParams mToolbarLayoutPrams = new LayoutParams(400, 80);
    private final FrameLayout.LayoutParams mHairlineLayoutParams = new LayoutParams(400, 5);
    @Mock private ControlContainer mControlContainer;
    @Mock private View mControlContainerView;
    @Mock private View mProgressBarContainer;
    @Mock private ViewGroup mProgressBarParent;

    private Context mContext;
    private final ObservableSupplierImpl<Boolean> mIsNtpShowing =
            new ObservableSupplierImpl<>(false);
    private final ObservableSupplierImpl<Boolean> mIsTabSwitcherShowing =
            new ObservableSupplierImpl<>(false);
    private final ObservableSupplierImpl<Boolean> mIsOmniboxFocused =
            new ObservableSupplierImpl<>(false);
    private final ObservableSupplierImpl<Boolean> mIsFindInPageShowing =
            new ObservableSupplierImpl<>(false);
    private final FormFieldFocusedSupplier mIsFormFieldFocused = new FormFieldFocusedSupplier();
    private BottomControlsStacker mBottomControlsStacker;
    private ToolbarPositionController mController;
    private final ObservableSupplierImpl<Integer> mBottomToolbarOffsetSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mKeyboardAccessoryHeightSupplier =
            new ObservableSupplierImpl<>(0);
    private final ObservableSupplierImpl<Integer> mControlContainerTranslationSupplier =
            new ObservableSupplierImpl<>(0);
    private final ObservableSupplierImpl<Integer> mControlContainerHeightSupplier =
            new ObservableSupplierImpl<>(LayoutParams.WRAP_CONTENT);
    private HistogramWatcher mStartupExpectation;
    private WindowAndroid mWindowAndroid;

    public static class FakeKeyboardVisibilityDelegate extends KeyboardVisibilityDelegate {
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

    private final FakeKeyboardVisibilityDelegate mKeyboardVisibilityDelegate =
            new FakeKeyboardVisibilityDelegate();

    @Before
    public void setUp() {
        doReturn(TOOLBAR_HEIGHT).when(mControlContainer).getToolbarHeight();
        doReturn(mControlContainerLayoutParams).when(mControlContainer).mutateLayoutParams();
        mHairlineLayoutParams.topMargin = TOOLBAR_HEIGHT;
        doReturn(mHairlineLayoutParams).when(mControlContainer).mutateHairlineLayoutParams();
        doReturn(mToolbarLayoutPrams).when(mControlContainer).mutateToolbarLayoutParams();
        doReturn(mControlContainerView).when(mControlContainer).getView();
        doReturn(CONTROL_CONTAINER_ID).when(mControlContainerView).getId();
        doReturn(mProgressBarLayoutParams).when(mProgressBarContainer).getLayoutParams();
        doReturn(mProgressBarParent).when(mProgressBarContainer).getParent();
        mContext = ContextUtils.getApplicationContext();
        doReturn(mContext.getResources()).when(mProgressBarContainer).getResources();
        mWindowAndroid = new WindowAndroid(mContext, false);
        mBottomControlsStacker =
                new BottomControlsStacker(mBrowserControlsSizer, mContext, mWindowAndroid);
        mBrowserControlsSizer.setControlsPosition(
                ControlsPosition.TOP, TOOLBAR_HEIGHT, 0, 0, 0, 0, 0);
        mControlContainerLayoutParams.gravity = Gravity.START | Gravity.TOP;
        mProgressBarLayoutParams.gravity = Gravity.TOP;
        mProgressBarLayoutParams.anchorGravity = Gravity.BOTTOM;
        mProgressBarLayoutParams.setAnchorId(CONTROL_CONTAINER_ID);

        ResettersForTesting.register(
                ToolbarPositionController::resetCachedToolbarConfigurationForTesting);
        mStartupExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.ToolbarPosition.PositionAtStartup", ControlsPosition.TOP);

        mController =
                new ToolbarPositionController(
                        mBrowserControlsSizer,
                        ContextUtils.getAppSharedPreferences(),
                        mIsNtpShowing,
                        mIsTabSwitcherShowing,
                        mIsOmniboxFocused,
                        mIsFormFieldFocused,
                        mIsFindInPageShowing,
                        mKeyboardAccessoryHeightSupplier,
                        mKeyboardVisibilityDelegate,
                        mControlContainer,
                        mBottomControlsStacker,
                        mBottomToolbarOffsetSupplier,
                        mProgressBarContainer,
                        mControlContainerTranslationSupplier,
                        mControlContainerHeightSupplier,
                        new Handler(Looper.getMainLooper()),
                        mContext);
    }

    @After
    public void tearDown() {
        mWindowAndroid.destroy();
    }

    /**
     * Simulate user changing toolbar anchor preference.
     *
     * @param showToolbarOnTop desired preference, or null to remove the key from settings,
     *     simulating the "default" state.
     */
    void setUserToolbarAnchorPreference(Boolean showToolbarOnTop) {
        var editor = ContextUtils.getAppSharedPreferences().edit();
        if (showToolbarOnTop == null) {
            editor.remove(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED);
            ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
        } else {
            editor.putBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, showToolbarOnTop);
        }
        editor.apply();
        ShadowLooper.runUiThreadTasks();
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
    @Config(qualifiers = "sw400dp", sdk = android.os.Build.VERSION_CODES.R)
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testIsToolbarPositionCustomizationEnabled_foldable() {
        ShadowPackageManager shadowPackageManager = Shadows.shadowOf(mContext.getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE, true);
        assertTrue(
                ToolbarPositionController.isToolbarPositionCustomizationEnabled(mContext, false));
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testMetrics() {
        mStartupExpectation.assertExpected();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Android.ToolbarPosition.PositionPrefChanged",
                                ControlsPosition.BOTTOM,
                                ControlsPosition.TOP)
                        .build();

        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ true);
        watcher.assertExpected();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR + ":default_to_top/false")
    public void testDefaultBottom() {
        assertControlsAtBottom();
        verify(mProgressBarContainer).setLayoutParams(mProgressBarLayoutParams);

        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ true);
        assertControlsAtTop();
        verify(mProgressBarContainer, times(2)).setLayoutParams(mProgressBarLayoutParams);
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testUpdatePositionChangesWithPref() {
        assertControlsAtTop();
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();

        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ true);
        assertControlsAtTop();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testUpdatePositionChangesWithNtpState() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
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
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
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
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();

        mIsOmniboxFocused.set(true);
        assertControlsAtTop();

        mIsOmniboxFocused.set(false);
        assertControlsAtBottom();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    @DisableFeatures(ChromeFeatureList.MINI_ORIGIN_BAR)
    public void testUpdatePositionChangesWithFormFieldFocusState() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();
        verify(mControlContainerView).setVisibility(View.INVISIBLE);

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        assertEquals(-TOOLBAR_HEIGHT, mBrowserControlsSizer.getTopControlOffset());
        assertControlsAtTop();
        verify(mControlContainerView, times(2)).setVisibility(View.INVISIBLE);

        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        assertEquals(TOOLBAR_HEIGHT, mBrowserControlsSizer.getBottomControlOffset());
        assertControlsAtBottom();

        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        assertControlsAtTop();

        mIsFormFieldFocused.onNodeAttributeUpdated(false, false);
        assertControlsAtBottom();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR, ChromeFeatureList.MINI_ORIGIN_BAR})
    public void testUpdatePositionFormField_MiniOriginBar() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        assertControlsAtBottom();
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testUpdatePositionChangesWithFindInPage() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
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
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();

        assertEquals(TOOLBAR_HEIGHT, mBottomControlsStacker.getTotalHeight());
        BottomControlsLayer toolbarLayer =
                mBottomControlsStacker.getLayerForTesting(LayerType.BOTTOM_TOOLBAR);
        assertEquals(TOOLBAR_HEIGHT, toolbarLayer.getHeight());
        assertEquals(LayerVisibility.VISIBLE, toolbarLayer.getLayerVisibility());
        assertEquals(LayerScrollBehavior.DEFAULT_SCROLL_OFF, toolbarLayer.getScrollBehavior());

        toolbarLayer.onBrowserControlsOffsetUpdate(12);
        verify(mControlContainerView).setTranslationY(12);
        assertEquals(12, mBottomToolbarOffsetSupplier.get().intValue());

        BottomControlsLayer progressBarLayer =
                mBottomControlsStacker.getLayerForTesting(LayerType.PROGRESS_BAR);
        assertEquals(0, progressBarLayer.getHeight());
        assertEquals(LayerVisibility.VISIBLE, progressBarLayer.getLayerVisibility());
        assertEquals(LayerScrollBehavior.DEFAULT_SCROLL_OFF, progressBarLayer.getScrollBehavior());

        progressBarLayer.onBrowserControlsOffsetUpdate(-12);
        verify(mProgressBarContainer).setTranslationY(-12);

        mIsOmniboxFocused.set(true);
        assertControlsAtTop();
        assertEquals(LayerVisibility.HIDDEN, toolbarLayer.getLayerVisibility());
        verify(mControlContainerView, atLeast(1)).setTranslationY(0);
        assertEquals(LayerVisibility.HIDDEN, progressBarLayer.getLayerVisibility());
        verify(mProgressBarContainer, atLeast(1)).setTranslationY(0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testGetToolbarPositionResId() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ true);
        assertEquals(
                R.string.address_bar_settings_top,
                ToolbarPositionController.getToolbarPositionResId());

        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertEquals(
                R.string.address_bar_settings_bottom,
                ToolbarPositionController.getToolbarPositionResId());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    @DisableFeatures(ChromeFeatureList.MINI_ORIGIN_BAR)
    public void testCalculateStateTransition() {
        boolean formFieldStateChanged = false;
        boolean prefStateChanged = false;
        boolean ntpShowing = false;
        boolean tabSwitcherShowing = false;
        boolean isOmniboxFocused = false;
        boolean isFindInPageShowing = false;
        boolean isFormFieldFocusedWithKeyboardVisible = false;
        boolean doesUserPreferTopToolbar = false;

        assertEquals(
                StateTransition.NONE,
                ToolbarPositionController.calculateStateTransition(
                        formFieldStateChanged,
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.BOTTOM));

        assertEquals(
                StateTransition.SNAP_TO_TOP,
                ToolbarPositionController.calculateStateTransition(
                        formFieldStateChanged,
                        prefStateChanged,
                        true,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.BOTTOM));

        assertEquals(
                StateTransition.SNAP_TO_TOP,
                ToolbarPositionController.calculateStateTransition(
                        formFieldStateChanged,
                        prefStateChanged,
                        ntpShowing,
                        true,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.BOTTOM));

        assertEquals(
                StateTransition.SNAP_TO_TOP,
                ToolbarPositionController.calculateStateTransition(
                        formFieldStateChanged,
                        prefStateChanged,
                        ntpShowing,
                        true,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.BOTTOM));

        assertEquals(
                StateTransition.SNAP_TO_TOP,
                ToolbarPositionController.calculateStateTransition(
                        formFieldStateChanged,
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        true,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.BOTTOM));

        assertEquals(
                StateTransition.SNAP_TO_TOP,
                ToolbarPositionController.calculateStateTransition(
                        formFieldStateChanged,
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        true,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.BOTTOM));

        assertEquals(
                StateTransition.SNAP_TO_TOP,
                ToolbarPositionController.calculateStateTransition(
                        formFieldStateChanged,
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        true,
                        doesUserPreferTopToolbar,
                        ControlsPosition.BOTTOM));

        assertEquals(
                StateTransition.SNAP_TO_BOTTOM,
                ToolbarPositionController.calculateStateTransition(
                        formFieldStateChanged,
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.TOP));

        assertEquals(
                StateTransition.ANIMATE_TO_BOTTOM,
                ToolbarPositionController.calculateStateTransition(
                        true,
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.TOP));

        assertEquals(
                StateTransition.ANIMATE_TO_BOTTOM,
                ToolbarPositionController.calculateStateTransition(
                        formFieldStateChanged,
                        true,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.TOP));
    }

    @Test
    public void shouldShowToolbarOnTop_withNtpUrl() {
        Tab tab = mock(Tab.class);
        doReturn(new GURL(UrlConstants.NTP_URL)).when(tab).getUrl();

        // By default, Toolbar should be anchored on top.
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ null);
        assertTrue(ToolbarPositionController.shouldShowToolbarOnTop(tab));

        // When the user explicitly asks for bottom toolbar, NTP should still show that toolbar on
        // top.
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertTrue(ToolbarPositionController.shouldShowToolbarOnTop(tab));

        // ... NTP always shows toolbar on top.
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ true);
        assertTrue(ToolbarPositionController.shouldShowToolbarOnTop(tab));
    }

    @Test
    public void shouldShowToolbarOnTop_withIncognitoNtpUrl() {
        Tab tab = mock(Tab.class);
        doReturn(new GURL(UrlConstants.NTP_URL)).when(tab).getUrl();
        doReturn(true).when(tab).isIncognitoBranded();

        // By default, Toolbar should be anchored on top.
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ null);
        assertTrue(ToolbarPositionController.shouldShowToolbarOnTop(tab));

        // When the user explicitly asks for bottom toolbar the incognito NTP should show toolbar on
        // the bottom.
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertFalse(ToolbarPositionController.shouldShowToolbarOnTop(tab));

        // ... same for the explicit top toolbar.
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ true);
        assertTrue(ToolbarPositionController.shouldShowToolbarOnTop(tab));
    }

    @Test
    public void shouldShowToolbarOnTop_withNonNtpUrl() {
        // This test does not instantiate ToolbarPositionController, meaning there is no Preference
        // observer watching for settings changes.
        Tab tab = mock(Tab.class);
        doReturn(new GURL(UrlConstants.ABOUT_URL)).when(tab).getUrl();

        // By default, Toolbar should be anchored on top.
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ null);
        ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
        assertTrue(ToolbarPositionController.shouldShowToolbarOnTop(tab));

        // When the user explicitly asks for bottom toolbar, non-NTP URLs should obey.
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
        assertFalse(ToolbarPositionController.shouldShowToolbarOnTop(tab));

        // ... same if the User wants explicitly Top toolbar.
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ true);
        ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
        assertTrue(ToolbarPositionController.shouldShowToolbarOnTop(tab));
    }

    @Test
    public void shouldShowToolbarOnTop_edgeCases() {
        // This test does not instantiate ToolbarPositionController, meaning there is no Preference
        // observer watching for settings changes.
        Tab tab = mock(Tab.class);

        // By default, Toolbar should be anchored on top.
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ null);
        ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
        assertTrue(ToolbarPositionController.shouldShowToolbarOnTop(null));
        assertTrue(ToolbarPositionController.shouldShowToolbarOnTop(tab));

        // In the absence of relevant signals, assume regular tab.
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
        assertFalse(ToolbarPositionController.shouldShowToolbarOnTop(null));
        assertFalse(ToolbarPositionController.shouldShowToolbarOnTop(tab));

        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ true);
        ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
        assertTrue(ToolbarPositionController.shouldShowToolbarOnTop(null));
        assertTrue(ToolbarPositionController.shouldShowToolbarOnTop(tab));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR, ChromeFeatureList.MINI_ORIGIN_BAR})
    public void testControlContainerTranslationAdjustments() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        assertControlsAtBottom();

        BottomControlsLayerWithOffset toolbarLayer =
                (BottomControlsLayerWithOffset)
                        mBottomControlsStacker.getLayerForTesting(LayerType.BOTTOM_TOOLBAR);
        toolbarLayer.onBrowserControlsOffsetUpdate(12);
        verify(mControlContainerView).setTranslationY(12);

        mKeyboardAccessoryHeightSupplier.set(100);
        verify(mControlContainerView).setTranslationY(12 - 100);
        assertEquals(12 - 100, mBottomToolbarOffsetSupplier.get().intValue());

        mKeyboardAccessoryHeightSupplier.set(0);
        verify(mControlContainerView, times(2)).setTranslationY(12);

        mControlContainerTranslationSupplier.set(10);
        verify(mControlContainerView).setTranslationY(22);
        assertEquals(22, mBottomToolbarOffsetSupplier.get().intValue());

        mControlContainerTranslationSupplier.set(20);
        verify(mControlContainerView).setTranslationY(32);
        assertEquals(32, mBottomToolbarOffsetSupplier.get().intValue());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR, ChromeFeatureList.MINI_ORIGIN_BAR})
    public void testParentLayoutInLayoutDuringPositionChange() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();

        doReturn(true).when(mProgressBarParent).isInLayout();
        mIsNtpShowing.set(true);

        // Progress bar params should not have changed yet; changing them mid-layout pass can cause
        // a crash.
        assertEquals(Gravity.BOTTOM, mProgressBarLayoutParams.gravity);
        assertEquals(Gravity.NO_GRAVITY, mProgressBarLayoutParams.anchorGravity);
        assertEquals(View.NO_ID, mProgressBarLayoutParams.getAnchorId());

        // Run the posted task to complete changing the progress bar layout params.
        ShadowLooper.idleMainLooper();
        assertControlsAtTop();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR, ChromeFeatureList.MINI_ORIGIN_BAR})
    public void testControlContainerHeightAdjustments() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        assertControlsAtBottom();
        assertEquals(TOOLBAR_HEIGHT, mBottomControlsStacker.getTotalHeight());

        mControlContainerHeightSupplier.set(15);
        assertEquals(15, mBottomControlsStacker.getTotalHeight());
        assertEquals(15, mHairlineLayoutParams.bottomMargin);
    }

    @Test
    public void testDestroy() {
        mController.destroy();
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtTop();
    }

    private void assertControlsAtBottom() {
        assertEquals(ControlsPosition.BOTTOM, mBrowserControlsSizer.getControlsPosition());
        assertEquals(0, mBrowserControlsSizer.getTopControlsHeight());
        assertEquals(TOOLBAR_HEIGHT, mBrowserControlsSizer.getBottomControlsHeight());
        assertEquals(TOOLBAR_HEIGHT, mHairlineLayoutParams.bottomMargin);
        assertEquals(Gravity.START | Gravity.BOTTOM, mControlContainerLayoutParams.gravity);
        assertEquals(1, mToolbarLayoutPrams.topMargin);
        assertEquals(Gravity.BOTTOM, mProgressBarLayoutParams.gravity);
        assertEquals(Gravity.NO_GRAVITY, mProgressBarLayoutParams.anchorGravity);
        assertEquals(View.NO_ID, mProgressBarLayoutParams.getAnchorId());
    }

    private void assertControlsAtTop() {
        assertEquals(ControlsPosition.TOP, mBrowserControlsSizer.getControlsPosition());
        assertEquals(TOOLBAR_HEIGHT, mBrowserControlsSizer.getTopControlsHeight());
        assertEquals(0, mBrowserControlsSizer.getBottomControlsHeight());
        assertEquals(TOOLBAR_HEIGHT, mHairlineLayoutParams.topMargin);
        assertEquals(0, mHairlineLayoutParams.bottomMargin);
        assertEquals(Gravity.START | Gravity.TOP, mControlContainerLayoutParams.gravity);
        assertEquals(Gravity.TOP, mProgressBarLayoutParams.gravity);
        assertEquals(Gravity.BOTTOM, mProgressBarLayoutParams.anchorGravity);
        assertEquals(CONTROL_CONTAINER_ID, mProgressBarLayoutParams.getAnchorId());
    }
}
