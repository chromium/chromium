// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.ToolbarPositionController.BOTTOM_OMNIBOX_EVER_USED_PREF;
import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Insets;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;

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

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.prefs.LocalStatePrefsJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.BottomControlsLayerWithOffset;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.StateTransition;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController.ToolbarPositionAndSource;
import org.chromium.chrome.browser.toolbar.settings.AddressBarPreference;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.ui.edge_to_edge.TopInsetProvider;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link ToolbarPositionController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ToolbarPositionControllerTest {

    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Mock Tab mTab;

    private static final int TOOLBAR_HEIGHT = 56;
    private static final int CONTROL_CONTAINER_ID = 12356;
    private static final int STATUS_BAR_HEIGHT = 10;

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
    private final CoordinatorLayout.LayoutParams mToolbarLayoutParams =
            new CoordinatorLayout.LayoutParams(400, 80);
    private final CoordinatorLayout.LayoutParams mHairlineLayoutParams =
            new CoordinatorLayout.LayoutParams(400, 5);
    @Mock private ControlContainer mControlContainer;
    @Mock private ToolbarLayout mToolbarLayout;
    @Mock private View mControlContainerView;
    @Mock private View mProgressBarContainer;
    @Mock private ViewGroup mProgressBarParent;
    @Mock private TopInsetProvider mTopInsetProvider;
    @Mock private View mRootView;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsNatives;
    @Mock private PrefService mPrefs;
    @Mock private LocalStatePrefs.Natives mLocalStatePrefsNatives;
    @Mock private PrefService mLocalPrefService;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private DisplayAndroid mDisplayAndroid;
    @Mock private InsetObserver mInsetObserver;

    private Context mContext;
    private final SettableNonNullObservableSupplier<Boolean> mIsNtpShowing =
            ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Boolean> mIsIncognitoNtpShowing =
            ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Boolean> mIsTabSwitcherShowing =
            ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Boolean> mIsOmniboxFocused =
            ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Boolean> mIsFindInPageShowing =
            ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Integer> mToolbarPosition =
            ObservableSuppliers.createNonNull(ControlsPosition.NONE);
    private final FormFieldFocusedSupplier mIsFormFieldFocused = new FormFieldFocusedSupplier();
    private BottomControlsStacker mBottomControlsStacker;
    private ToolbarPositionController mController;
    private final SettableNonNullObservableSupplier<Integer> mBottomToolbarOffsetSupplier =
            ObservableSuppliers.createNonNull(0);
    private final SettableNonNullObservableSupplier<Integer> mKeyboardAccessoryHeightSupplier =
            ObservableSuppliers.createNonNull(0);
    private final SettableNonNullObservableSupplier<Integer> mControlContainerTranslationSupplier =
            ObservableSuppliers.createNonNull(0);
    private final SettableNonNullObservableSupplier<Integer> mControlContainerHeightSupplier =
            ObservableSuppliers.createNonNull(TOOLBAR_HEIGHT);
    private final SettableNonNullObservableSupplier<Integer> mKeyboardHeightSupplier =
            ObservableSuppliers.createNonNull(0);
    private SettableNonNullObservableSupplier<Profile> mProfileSupplier;
    private final SettableMonotonicObservableSupplier<Tab> mActivityTabSupplier =
            ObservableSuppliers.createMonotonic();
    private HistogramWatcher mStartupExpectation;

    public static class FakeKeyboardVisibilityDelegate extends KeyboardVisibilityDelegate {
        private boolean mIsShowing;

        public void setVisibilityForTests(boolean isShowing) {
            mIsShowing = isShowing;
            notifyListeners(isShowing);
        }

        @Override
        public boolean isKeyboardShowing(View view) {
            return mIsShowing;
        }
    }

    private final FakeKeyboardVisibilityDelegate mKeyboardVisibilityDelegate =
            new FakeKeyboardVisibilityDelegate();

    @Before
    public void setUp() {
        doReturn(TOOLBAR_HEIGHT).when(mControlContainer).getToolbarHeight();
        doReturn(mControlContainerLayoutParams).when(mControlContainer).mutateLayoutParams();
        mHairlineLayoutParams.anchorGravity = Gravity.BOTTOM;
        mHairlineLayoutParams.gravity = Gravity.BOTTOM;
        mToolbarLayoutParams.bottomMargin = 1;
        doReturn(mHairlineLayoutParams).when(mControlContainer).mutateHairlineLayoutParams();
        doReturn(mToolbarLayoutParams).when(mControlContainer).mutateToolbarLayoutParams();
        doReturn(mControlContainerView).when(mControlContainer).getView();
        doReturn(CONTROL_CONTAINER_ID).when(mControlContainerView).getId();
        doReturn(mProgressBarLayoutParams).when(mProgressBarContainer).getLayoutParams();
        doReturn(mProgressBarParent).when(mProgressBarContainer).getParent();
        mContext = ContextUtils.getApplicationContext();
        doReturn(mContext.getResources()).when(mProgressBarContainer).getResources();
        mBottomControlsStacker =
                new BottomControlsStacker(mBrowserControlsSizer, mContext, mWindowAndroid);
        mBrowserControlsSizer.setControlsPosition(
                ControlsPosition.TOP, TOOLBAR_HEIGHT, 0, 0, 0, 0, 0);
        mControlContainerLayoutParams.gravity = Gravity.START | Gravity.TOP;
        mProgressBarLayoutParams.gravity = Gravity.BOTTOM;
        mProgressBarLayoutParams.anchorGravity = Gravity.BOTTOM;
        mProgressBarLayoutParams.setAnchorId(CONTROL_CONTAINER_ID);
        mProfileSupplier = ObservableSuppliers.createNonNull(mProfile);
        UserPrefsJni.setInstanceForTesting(mUserPrefsNatives);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefs);
        doReturn(mInsetObserver).when(mWindowAndroid).getInsetObserver();

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
                        mIsIncognitoNtpShowing,
                        mIsTabSwitcherShowing,
                        mIsOmniboxFocused,
                        mIsFormFieldFocused.getObservable(),
                        mIsFindInPageShowing,
                        mKeyboardAccessoryHeightSupplier,
                        mKeyboardVisibilityDelegate,
                        mControlContainer,
                        mToolbarLayout,
                        mBottomControlsStacker,
                        mBottomToolbarOffsetSupplier,
                        mProgressBarContainer,
                        mControlContainerTranslationSupplier,
                        mControlContainerHeightSupplier,
                        mTopInsetProvider,
                        new Handler(Looper.getMainLooper()),
                        mContext,
                        mToolbarPosition,
                        mProfileSupplier,
                        mActivityTabSupplier,
                        mKeyboardHeightSupplier,
                        mWindowAndroid);

        LocalStatePrefs.setNativePrefsLoadedForTesting(true);
        LocalStatePrefsJni.setInstanceForTesting(mLocalStatePrefsNatives);
        when(mLocalStatePrefsNatives.getPrefService()).thenReturn(mLocalPrefService);

        AtomicReference<@Nullable Boolean> localPrefValue = new AtomicReference<>();
        doAnswer(
                        invocation -> {
                            localPrefValue.set(invocation.getArgument(1));
                            return null;
                        })
                .when(mLocalPrefService)
                .setBoolean(eq(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION), anyBoolean());
        when(mLocalPrefService.hasPrefPath(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION))
                .thenAnswer(invocation -> localPrefValue.get() != null);
        when(mLocalPrefService.getBoolean(Pref.IS_OMNIBOX_IN_BOTTOM_POSITION))
                .thenAnswer(invocation -> localPrefValue.get() != null && localPrefValue.get());
    }

    /**
     * Simulate user changing toolbar anchor preference.
     *
     * @param showToolbarOnTop desired preference, or null to remove the key from settings,
     *     simulating the "default" state.
     */
    void setUserToolbarAnchorPreference(Boolean showToolbarOnTop) {
        if (showToolbarOnTop == null) {
            AddressBarPreference.setToolbarPositionAndSource(ToolbarPositionAndSource.TOP_SETTINGS);
            ToolbarPositionController.resetCachedToolbarConfigurationForTesting();
        } else {
            AddressBarPreference.setToolbarPositionAndSource(
                    showToolbarOnTop
                            ? ToolbarPositionAndSource.TOP_LONG_PRESS
                            : ToolbarPositionAndSource.BOTTOM_LONG_PRESS);
        }
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @Test
    @Config(qualifiers = "ldltr-sw600dp")
    public void testIsToolbarPositionCustomizationEnabled_tablet() {
        assertFalse(
                ToolbarPositionController.isToolbarPositionCustomizationEnabled(mContext, false));
    }

    @Test
    @Config(qualifiers = "sw400dp")
    public void testIsToolbarPositionCustomizationEnabled_phone() {
        assertFalse(
                ToolbarPositionController.isToolbarPositionCustomizationEnabled(mContext, true));
        assertTrue(
                ToolbarPositionController.isToolbarPositionCustomizationEnabled(mContext, false));
    }

    @Test
    @Config(qualifiers = "sw400dp", sdk = android.os.Build.VERSION_CODES.R)
    public void testIsToolbarPositionCustomizationEnabled_foldable() {
        ShadowPackageManager shadowPackageManager = Shadows.shadowOf(mContext.getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE, true);
        assertTrue(
                ToolbarPositionController.isToolbarPositionCustomizationEnabled(mContext, false));
    }

    @Test
    @Config(qualifiers = "sw400dp")
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
    public void testUpdatePositionChangesWithPref() {
        assertControlsAtTop();
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();
        verify(mPrefs, times(1)).setBoolean(eq(BOTTOM_OMNIBOX_EVER_USED_PREF), eq(true));

        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ true);
        assertControlsAtTop();
        verify(mPrefs, times(1)).setBoolean(eq(BOTTOM_OMNIBOX_EVER_USED_PREF), eq(true));
    }

    @Test
    @Config(qualifiers = "sw400dp")
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
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2)
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
    public void testUpdatePositionFormField_MiniOriginBar() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        assertControlsAtBottom();
    }

    @Test
    @Config(qualifiers = "sw400dp")
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
    public void testBottomControlsStacker() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();

        assertEquals(TOOLBAR_HEIGHT, mBottomControlsStacker.getTotalHeight());
    }

    @Test
    @Config(qualifiers = "sw400dp")
    public void testBottomControlsStacker_toolbarLayer() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();

        BottomControlsLayer toolbarLayer =
                mBottomControlsStacker.getLayerForTesting(LayerType.BOTTOM_TOOLBAR);
        assertEquals(TOOLBAR_HEIGHT, toolbarLayer.getHeight());
        assertEquals(LayerVisibility.VISIBLE, toolbarLayer.getLayerVisibility());
        assertEquals(LayerScrollBehavior.DEFAULT_SCROLL_OFF, toolbarLayer.getScrollBehavior());

        toolbarLayer.onBrowserControlsOffsetUpdate(12);
        verify(mControlContainerView).setTranslationY(12);
        assertEquals(12, mBottomToolbarOffsetSupplier.get().intValue());
    }

    @Test
    @Config(qualifiers = "sw400dp")
    public void testBottomControlsStacker_progressBarLayer() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();

        BottomControlsLayer progressBarLayer =
                mBottomControlsStacker.getLayerForTesting(LayerType.PROGRESS_BAR);
        assertEquals(0, progressBarLayer.getHeight());
        assertEquals(LayerVisibility.VISIBLE, progressBarLayer.getLayerVisibility());
        assertEquals(LayerScrollBehavior.DEFAULT_SCROLL_OFF, progressBarLayer.getScrollBehavior());

        progressBarLayer.onBrowserControlsOffsetUpdate(-12);
        verify(mProgressBarContainer).setTranslationY(-12);
    }

    @Test
    @Config(qualifiers = "sw400dp")
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2)
    public void testBottomControlsStacker_visibilityChanges() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtBottom();

        BottomControlsLayer toolbarLayer =
                mBottomControlsStacker.getLayerForTesting(LayerType.BOTTOM_TOOLBAR);
        BottomControlsLayer progressBarLayer =
                mBottomControlsStacker.getLayerForTesting(LayerType.PROGRESS_BAR);

        mIsOmniboxFocused.set(true);
        assertControlsAtTop();

        assertEquals(LayerVisibility.HIDDEN, toolbarLayer.getLayerVisibility());
        verify(mControlContainerView, atLeast(1)).setTranslationY(0);
        assertEquals(LayerVisibility.HIDDEN, progressBarLayer.getLayerVisibility());
        verify(mProgressBarContainer, atLeast(1)).setTranslationY(0);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2})
    public void testCalculateStateTransition() {
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
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        true,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.BOTTOM));

        assertEquals(
                StateTransition.NONE,
                ToolbarPositionController.calculateStateTransition(
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
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.TOP));

        assertEquals(
                StateTransition.SNAP_TO_BOTTOM,
                ToolbarPositionController.calculateStateTransition(
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.TOP));

        AddressBarPreference.setToolbarPositionAndSource(
                ToolbarPositionAndSource.BOTTOM_LONG_PRESS);
        assertEquals(
                StateTransition.ANIMATE_TO_BOTTOM,
                ToolbarPositionController.calculateStateTransition(
                        true,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.TOP));

        AddressBarPreference.setToolbarPositionAndSource(ToolbarPositionAndSource.BOTTOM_SETTINGS);
        assertEquals(
                StateTransition.SNAP_TO_BOTTOM,
                ToolbarPositionController.calculateStateTransition(
                        true,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.TOP));

        AddressBarPreference.setToolbarPositionAndSource(ToolbarPositionAndSource.TOP_LONG_PRESS);
        assertEquals(
                StateTransition.ANIMATE_TO_TOP,
                ToolbarPositionController.calculateStateTransition(
                        true,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        true,
                        ControlsPosition.BOTTOM));

        AddressBarPreference.setToolbarPositionAndSource(ToolbarPositionAndSource.TOP_SETTINGS);
        assertEquals(
                StateTransition.SNAP_TO_TOP,
                ToolbarPositionController.calculateStateTransition(
                        true,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        true,
                        ControlsPosition.BOTTOM));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2)
    public void testForceBottomForFocusedOmnibox() {
        ChromeFeatureList.sAndroidBottomToolbarV2ForceBottomForFocusedOmnibox.setForTesting(true);
        boolean prefStateChanged = false;
        boolean ntpShowing = true;
        boolean tabSwitcherShowing = false;
        boolean isOmniboxFocused = true;
        boolean isFindInPageShowing = false;
        boolean isFormFieldFocusedWithKeyboardVisible = false;
        boolean doesUserPreferTopToolbar = true;

        assertEquals(
                StateTransition.SNAP_TO_BOTTOM,
                ToolbarPositionController.calculateStateTransition(
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.TOP));

        ChromeFeatureList.sAndroidBottomToolbarV2ForceBottomForFocusedOmnibox.setForTesting(false);
        doesUserPreferTopToolbar = false;

        assertEquals(
                StateTransition.SNAP_TO_BOTTOM,
                ToolbarPositionController.calculateStateTransition(
                        prefStateChanged,
                        ntpShowing,
                        tabSwitcherShowing,
                        isOmniboxFocused,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.TOP));

        AddressBarPreference.setToolbarPositionAndSource(
                ToolbarPositionAndSource.BOTTOM_LONG_PRESS);
        assertEquals(
                StateTransition.SNAP_TO_BOTTOM,
                ToolbarPositionController.calculateStateTransition(
                        true,
                        ntpShowing,
                        tabSwitcherShowing,
                        true,
                        isFindInPageShowing,
                        isFormFieldFocusedWithKeyboardVisible,
                        doesUserPreferTopToolbar,
                        ControlsPosition.TOP));
    }

    @Test
    public void shouldShowToolbarOnTop_withNtpUrl() {
        Tab tab = mock(Tab.class);
        doReturn(new GURL(getOriginalNativeNtpUrl())).when(tab).getUrl();

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
        doReturn(new GURL(getOriginalNativeNtpUrl())).when(tab).getUrl();
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
    public void testControlContainerTranslationAdjustments() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        assertControlsAtBottom();

        int baseTranslation = 12;
        BottomControlsLayerWithOffset toolbarLayer =
                (BottomControlsLayerWithOffset)
                        mBottomControlsStacker.getLayerForTesting(LayerType.BOTTOM_TOOLBAR);
        toolbarLayer.onBrowserControlsOffsetUpdate(baseTranslation);
        verify(mControlContainerView).setTranslationY(baseTranslation);

        final int chinHeight = 36;
        int keyboardAccessoryHeight = 100;
        mKeyboardAccessoryHeightSupplier.set(keyboardAccessoryHeight);
        mBottomControlsStacker.addLayer(
                new BottomControlsLayer() {
                    @Override
                    public int getType() {
                        return LayerType.BOTTOM_CHIN;
                    }

                    @Override
                    public int getScrollBehavior() {
                        return LayerScrollBehavior.DEFAULT_SCROLL_OFF;
                    }

                    @Override
                    public int getHeight() {
                        return chinHeight;
                    }

                    @Override
                    public int getLayerVisibility() {
                        return LayerVisibility.VISIBLE;
                    }
                });
        mBottomControlsStacker.requestLayerUpdate(false);
        toolbarLayer.onBrowserControlsOffsetUpdate(baseTranslation);
        verify(mControlContainerView).setTranslationY(baseTranslation + chinHeight);
        assertEquals(baseTranslation + chinHeight, mBottomToolbarOffsetSupplier.get().intValue());

        mKeyboardAccessoryHeightSupplier.set(0);
        mControlContainerTranslationSupplier.set(10);
        verify(mControlContainerView).setTranslationY(baseTranslation + 10);
        assertEquals(baseTranslation + 10, mBottomToolbarOffsetSupplier.get().intValue());

        mControlContainerTranslationSupplier.set(20);
        verify(mControlContainerView).setTranslationY(baseTranslation + 20);
        assertEquals(baseTranslation + 20, mBottomToolbarOffsetSupplier.get().intValue());
    }

    @Test
    public void testKeyboardAccessoryHeight() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        assertControlsAtBottom();
        int keyboardAccessoryHeight = 100;
        mKeyboardAccessoryHeightSupplier.set(keyboardAccessoryHeight);

        assertEquals(100, mControlContainerLayoutParams.bottomMargin);
    }

    @Test
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
        RobolectricUtil.runAllBackgroundAndUi();
        assertControlsAtTop();
    }

    @Test
    public void testOnToEdgeChange() {
        mActivityTabSupplier.set(mock(Tab.class));
        int topInset = 50;

        // Test case to apply the top inset.
        mController.onToEdgeChange(topInset, /* consumeTopInset= */ true, LayoutType.BROWSING);
        // Verifies that the topInset is sent to toolbar as a top padding.
        verify(mToolbarLayout).onToEdgeChange(eq(topInset));

        // Test case to remove the top inset.
        mController.onToEdgeChange(topInset, /* consumeTopInset= */ false, LayoutType.BROWSING);
        verify(mToolbarLayout).onToEdgeChange(eq(0));
    }

    @Test
    public void testOnToEdgeChange_NullTab_ReturnEarly() {
        // mActivityTabSupplier is not set, so the active tab is null.
        int topInset = 50;

        boolean result =
                mController.onToEdgeChange(
                        topInset, /* consumeTopInset= */ true, LayoutType.BROWSING);

        assertFalse(result);
        verify(mToolbarLayout, never()).onToEdgeChange(anyInt());
    }

    @Test
    public void testOnToEdgeChange_NullTab_ToolbarSwipe() {
        // mActivityTabSupplier is not set, so the active tab is null.
        int topInset = 50;

        boolean result =
                mController.onToEdgeChange(
                        topInset, /* consumeTopInset= */ true, LayoutType.TOOLBAR_SWIPE);

        // Toolbar swipe should NOT return early even with a null tab.
        assertTrue(result);
        verify(mToolbarLayout).onToEdgeChange(eq(topInset));
    }

    @Test
    public void testControlContainerHeightAdjustments() {
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        assertControlsAtBottom();
        assertEquals(TOOLBAR_HEIGHT, mBottomControlsStacker.getTotalHeight());

        mControlContainerHeightSupplier.set(15);
        assertEquals(15, mBottomControlsStacker.getTotalHeight());
    }

    @Test
    @Config(qualifiers = "sw400dp", sdk = 30)
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2)
    public void testBottomAnchoredFocusedOmnibox() {
        // Null InsetObserver, should fallback to keyboard's overlay mode.
        doReturn(null).when(mWindowAndroid).getInsetObserver();
        setupBottomAnchoredFocusedOmniboxTest();

        mKeyboardHeightSupplier.set(400);
        verify(mControlContainerView).setTranslationY(-400f);

        // If the window is too short to accommodate the keyboard + the full height of the toolbar,
        // the toolbar should be translated up to the top of the screen but no further.
        doReturn(430).when(mDisplayAndroid).getDisplayHeight();
        mKeyboardHeightSupplier.set(401);
        verify(mControlContainerView)
                .setTranslationY(-(430f - TOOLBAR_HEIGHT - STATUS_BAR_HEIGHT));
        verify(mControlContainer, atLeast(1)).setMaxHeight(20);
    }

    @Test
    @Config(qualifiers = "sw400dp", sdk = 30)
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2)
    public void testBottomAnchoredFocusedOmnibox_whenOverlayKeyboardMode() {
        doReturn(true).when(mInsetObserver).isKeyboardInOverlayMode();
        setupBottomAnchoredFocusedOmniboxTest();

        mKeyboardHeightSupplier.set(400);
        verify(mControlContainerView, atLeast(1)).setTranslationY(-400f);
    }

    @Test
    @Config(qualifiers = "sw400dp", sdk = 30)
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR_V2)
    public void testBottomAnchoredFocusedOmnibox_whenResizeKeyboardMode() {
        doReturn(false).when(mInsetObserver).isKeyboardInOverlayMode();
        setupBottomAnchoredFocusedOmniboxTest();

        mKeyboardHeightSupplier.set(400);
        // In resize mode, keyboard height should be ignored for translation.
        verify(mControlContainerView, atLeast(1)).setTranslationY(0f);
    }

    @Test
    public void testDestroy() {
        mController.destroy();
        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        assertControlsAtTop();
    }

    private void setupBottomAnchoredFocusedOmniboxTest() {
        doReturn(mDisplayAndroid).when(mWindowAndroid).getDisplay();
        doReturn(1000).when(mDisplayAndroid).getDisplayHeight();
        doReturn(mRootView).when(mControlContainerView).getRootView();

        WindowInsets rootViewInsets =
                new WindowInsets.Builder()
                        .setInsets(WindowInsets.Type.ime(), Insets.of(0, 0, 0, 400))
                        .setInsets(
                                WindowInsets.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_HEIGHT, 0, 0))
                        .build();
        doReturn(rootViewInsets).when(mControlContainerView).getRootWindowInsets();

        setUserToolbarAnchorPreference(/* showToolbarOnTop= */ false);
        mIsOmniboxFocused.set(true);
        assertControlsAtBottom();
    }

    @Test
    public void testMaybeForceBottomToolbarLayoutUpdateAndCapture() {
        // 1. Test mIsFirstPositionChange is true.
        assertTrue(mController.getIsFirstPositionChangeForTesting());
        // After setUp, mIsFirstPositionChange is true because initial position (TOP) didn't change.
        mController.maybeForceBottomToolbarLayoutUpdateAndCapture(/* isNtpShowing= */ true);
        verify(mControlContainer, never()).doSynchronousLayoutAndCapture();

        // Trigger a position change to set mIsFirstPositionChange to false.
        setUserToolbarAnchorPreference(false); // Changes to BOTTOM
        assertControlsAtBottom();
        // During this first change, maybeForceToolbarLayoutUpdateAndCapture() was called inside
        // updateCurrentPosition(), but mIsFirstPositionChange was still true, so it did nothing.
        verify(mControlContainer, never()).doSynchronousLayoutAndCapture();

        // mIsFirstPositionChange is now false.
        assertFalse(mController.getIsFirstPositionChangeForTesting());

        // 2. Test active tab is NTP.
        mController.maybeForceBottomToolbarLayoutUpdateAndCapture(/* isNtpShowing= */ true);
        verify(mControlContainer, never()).doSynchronousLayoutAndCapture();

        // 3. Test active tab is not NTP, and layout changed.
        // We need onToEdgeChange to return true.
        // maybeForceToolbarLayoutUpdateAndCapture calls onToEdgeChange(0, false,
        // LayoutType.BROWSING).
        // Set mTopInset to something non-zero first.
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        mActivityTabSupplier.set(mTab);
        mController.onToEdgeChange(50, true, LayoutType.BROWSING);
        mController.maybeForceBottomToolbarLayoutUpdateAndCapture(/* isNtpShowing= */ false);
        verify(mControlContainer).doSynchronousLayoutAndCapture();

        // 4. Test active tab is not NTP, but layout DID NOT change.
        // mTopInset is now 0 (from previous call).
        clearInvocations(mControlContainer);
        mController.maybeForceBottomToolbarLayoutUpdateAndCapture(/* isNtpShowing= */ false);
        verify(mControlContainer, never()).doSynchronousLayoutAndCapture();
    }

    private void assertControlsAtBottom() {
        assertEquals(ControlsPosition.BOTTOM, mBrowserControlsSizer.getControlsPosition());
        assertEquals(0, mBrowserControlsSizer.getTopControlsHeight());
        assertEquals(TOOLBAR_HEIGHT, mBrowserControlsSizer.getBottomControlsHeight());
        assertEquals(Gravity.TOP, mHairlineLayoutParams.anchorGravity);
        assertEquals(Gravity.TOP, mHairlineLayoutParams.gravity);
        assertEquals(Gravity.START | Gravity.BOTTOM, mControlContainerLayoutParams.gravity);
        assertEquals(1, mToolbarLayoutParams.topMargin);
        assertEquals(Gravity.BOTTOM, mProgressBarLayoutParams.gravity);
        assertEquals(Gravity.NO_GRAVITY, mProgressBarLayoutParams.anchorGravity);
        assertEquals(View.NO_ID, mProgressBarLayoutParams.getAnchorId());
    }

    private void assertControlsAtTop() {
        assertEquals(ControlsPosition.TOP, mBrowserControlsSizer.getControlsPosition());
        assertEquals(TOOLBAR_HEIGHT, mBrowserControlsSizer.getTopControlsHeight());
        assertEquals(0, mBrowserControlsSizer.getBottomControlsHeight());
        assertEquals(Gravity.BOTTOM, mHairlineLayoutParams.anchorGravity);
        assertEquals(Gravity.BOTTOM, mHairlineLayoutParams.gravity);
        assertEquals(Gravity.START | Gravity.TOP, mControlContainerLayoutParams.gravity);
        assertEquals(1, mToolbarLayoutParams.bottomMargin);
        assertEquals(Gravity.BOTTOM, mProgressBarLayoutParams.gravity);
        assertEquals(Gravity.BOTTOM, mProgressBarLayoutParams.anchorGravity);
        assertEquals(CONTROL_CONTAINER_ID, mProgressBarLayoutParams.getAnchorId());
    }
}
