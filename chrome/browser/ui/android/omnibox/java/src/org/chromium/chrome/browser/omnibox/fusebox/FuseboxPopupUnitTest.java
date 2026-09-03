// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.clearInvocations;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.widget.PopupWindow.OnDismissListener;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.window.layout.WindowMetrics;
import androidx.window.layout.WindowMetricsCalculator;
import androidx.window.layout.WindowMetricsCalculatorDecorator;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.PopupState;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

import java.util.Locale;

/** Unit tests for FuseboxPopup. */
@RunWith(BaseRobolectricTestRunner.class)
public class FuseboxPopupUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private AnchoredPopupWindow mPopupWindow;
    @Mock private DynamicRectProvider mDynamicRectProvider;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private InsetObserver mInsetObserver;
    @Mock private WindowInsetsCompat mWindowInsets;
    @Mock private WindowMetricsCalculator mWindowMetricsCalculator;

    @Captor private ArgumentCaptor<RectProvider.Observer> mObserverCaptor;
    @Captor private ArgumentCaptor<OnDismissListener> mDismissListenerCaptor;

    private Activity mActivity;
    private FuseboxPopup mFuseboxPopup;
    private View mContentView;
    private ViewGroup mViewGroup;

    @Before
    public void setUp() {
        WindowMetricsCalculator.overrideDecorator(
                new WindowMetricsCalculatorDecorator() {
                    @Override
                    public WindowMetricsCalculator decorate(WindowMetricsCalculator calculator) {
                        return mWindowMetricsCalculator;
                    }
                });

        mActivity = Robolectric.setupActivity(TestActivity.class);
        mContentView = LayoutInflater.from(mActivity).inflate(R.layout.fusebox_context_popup, null);
        mActivity.setContentView(mContentView);
        mViewGroup = mContentView.findViewById(R.id.fusebox_view_group);

        when(mWindowAndroid.getInsetObserver()).thenReturn(mInsetObserver);

        mFuseboxPopup =
                new FuseboxPopup(
                        mActivity,
                        mWindowAndroid,
                        mPopupWindow,
                        mContentView,
                        mDynamicRectProvider,
                        /* isBottomSheet= */ false);
    }

    @After
    public void tearDown() {
        RobolectricUtil.runAllBackgroundAndUi();
        WindowMetricsCalculator.overrideDecorator(
                new WindowMetricsCalculatorDecorator() {
                    @Override
                    public WindowMetricsCalculator decorate(WindowMetricsCalculator calculator) {
                        return calculator;
                    }
                });
    }

    private void recreateFuseboxPopup(boolean isBottomSheet) {
        mContentView = LayoutInflater.from(mActivity).inflate(R.layout.fusebox_context_popup, null);
        mActivity.setContentView(mContentView);
        mFuseboxPopup =
                new FuseboxPopup(
                        mActivity,
                        mWindowAndroid,
                        mPopupWindow,
                        mContentView,
                        mDynamicRectProvider,
                        isBottomSheet);
    }

    private void setupMultiWindowMetrics(
            Rect currentBounds, Rect maxBounds, int navBarBottomInset) {
        WindowMetrics windowMetrics = new WindowMetrics(currentBounds, 1.0f);
        WindowMetrics maxWindowMetrics = new WindowMetrics(maxBounds, 1.0f);
        when(mWindowMetricsCalculator.computeCurrentWindowMetrics(mActivity))
                .thenReturn(windowMetrics);
        when(mWindowMetricsCalculator.computeMaximumWindowMetrics(mActivity))
                .thenReturn(maxWindowMetrics);

        Insets navBarInsets = Insets.of(0, 0, 0, navBarBottomInset);
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(mWindowInsets);
        when(mWindowInsets.getInsets(WindowInsetsCompat.Type.navigationBars()))
                .thenReturn(navBarInsets);
    }

    @Test
    public void testSetPopupState_Hidden() {
        mFuseboxPopup.setPopupState(PopupState.HIDDEN);
        verify(mDynamicRectProvider).setPopupState(PopupState.HIDDEN);
        verify(mPopupWindow).dismiss();
    }

    @Test
    public void testSetPopupState_Floating() {
        mFuseboxPopup.setPopupState(PopupState.FLOATING);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mDynamicRectProvider).setPopupState(PopupState.FLOATING);
        verify(mPopupWindow).show();
    }

    @Test
    public void testSetPopupState_firstShow_updatesDesiredWidthBeforeShowing() {
        doReturn(250).when(mDynamicRectProvider).getPopupWidth(eq(PopupState.FLOATING), any());

        mFuseboxPopup.setPopupState(PopupState.FLOATING);

        // Desired width is updated synchronously on first show before the show task runs.
        verify(mPopupWindow)
                .updateDesiredContentSize(
                        /* width= */ 250, /* height= */ 0, /* updateLayout= */ true);
        verify(mPopupWindow, never()).show();

        RobolectricUtil.runAllBackgroundAndUi();
        verify(mPopupWindow).show();
    }

    @Test
    public void testSetPopupState_subsequentShow_showsImmediately() {
        mFuseboxPopup.setPopupState(PopupState.FLOATING);
        RobolectricUtil.runAllBackgroundAndUi();

        mFuseboxPopup.setPopupState(PopupState.HIDDEN);
        clearInvocations(mPopupWindow);

        mFuseboxPopup.setPopupState(PopupState.FLOATING);
        // On subsequent show, show() is invoked immediately without needing task posting.
        verify(mPopupWindow).show();
    }

    @Test
    public void testSetPopupState_Bottom() {
        mFuseboxPopup.setPopupState(PopupState.BOTTOM);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mDynamicRectProvider).setPopupState(PopupState.BOTTOM);
        verify(mPopupWindow).show();
    }

    @Test
    public void testSetPopupState_Bottom_setsAnimation() {
        mFuseboxPopup.setPopupState(PopupState.BOTTOM);
        verify(mPopupWindow).setAnimationStyle(R.style.FuseboxBottomSheetAnimation);
    }

    @Test
    public void testSetPopupState_Floating_clearsAnimation() {
        mFuseboxPopup.setPopupState(PopupState.FLOATING);
        verify(mPopupWindow).setAnimationStyle(0);
    }

    @Test
    public void testSetPopupState_Bottom_blocksBackgroundAccessibility() {
        View contentView = mActivity.findViewById(android.R.id.content);
        assertNotNull(contentView);
        contentView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
        mFuseboxPopup.setPopupState(PopupState.BOTTOM);

        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS,
                contentView.getImportantForAccessibility());
    }

    @Test
    public void testSetPopupState_Floating_doesNotBlockBackgroundAccessibility() {
        View contentView = mActivity.findViewById(android.R.id.content);
        assertNotNull(contentView);
        contentView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
        mFuseboxPopup.setPopupState(PopupState.FLOATING);

        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_AUTO, contentView.getImportantForAccessibility());
    }

    @Test
    public void testDismiss_restoresBackgroundAccessibility() {
        View contentView = mActivity.findViewById(android.R.id.content);
        assertNotNull(contentView);
        contentView.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);

        mFuseboxPopup.setPopupState(PopupState.BOTTOM);
        verify(mPopupWindow).addOnDismissListener(mDismissListenerCaptor.capture());

        mDismissListenerCaptor.getValue().onDismiss();

        assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_AUTO, contentView.getImportantForAccessibility());
    }

    @Test
    public void testDynamicInflation_VerticalLayout() {
        OmniboxFeatures.setShowBottomSheetPopupForTesting(false);

        // Re-create content view and popup to trigger new inflation logic
        recreateFuseboxPopup(/* isBottomSheet= */ false);

        // Verify that we can find the elements
        assertNotNull(mFuseboxPopup.mAddCurrentTab);
        assertNotNull(mFuseboxPopup.mTabButton);
        assertNotNull(mFuseboxPopup.mCameraButton);
        assertNotNull(mFuseboxPopup.mGalleryButton);
        assertNotNull(mFuseboxPopup.mFileButton);
    }

    @Test
    public void testDynamicInflation_HorizontalLayout() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        OmniboxFeatures.setShowBottomSheetPopupForTesting(true);

        // Re-create content view and popup to trigger new inflation logic
        recreateFuseboxPopup(/* isBottomSheet= */ true);

        // Verify that we can find the elements
        assertNotNull(mFuseboxPopup.mAddCurrentTab);
        assertNotNull(mFuseboxPopup.mTabButton);
        assertNotNull(mFuseboxPopup.mCameraButton);
        assertNotNull(mFuseboxPopup.mGalleryButton);
        assertNotNull(mFuseboxPopup.mFileButton);
    }

    @Test
    public void testUpdateLayout() {
        doReturn(100).when(mDynamicRectProvider).getPopupWidth(eq(PopupState.FLOATING), any());

        doReturn(true).when(mPopupWindow).isShowing();

        mFuseboxPopup.setPopupState(PopupState.FLOATING);

        RobolectricUtil.runAllBackgroundAndUi();

        verify(mPopupWindow, atLeastOnce()).updateDesiredContentSize(100, 0, true);
    }

    @Test
    public void testUpdateInsets_ImeVisible() {
        doReturn(true).when(mPopupWindow).isShowing();
        mFuseboxPopup.setPopupState(PopupState.FLOATING);

        // First layout update.
        mFuseboxPopup.updateLayout();
        assertEquals(0, mFuseboxPopup.mScrollView.getPaddingBottom());

        // Second layout update to test idempotency.
        mFuseboxPopup.updateLayout();
        assertEquals(0, mFuseboxPopup.mScrollView.getPaddingBottom());
    }

    @Test
    public void testUpdateInsets_BottomSheet() {
        Insets navBarInsets = Insets.of(0, 0, 0, 50);
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(mWindowInsets);
        when(mWindowInsets.getInsets(WindowInsetsCompat.Type.navigationBars()))
                .thenReturn(navBarInsets);
        doReturn(true).when(mPopupWindow).isShowing();

        mFuseboxPopup.setPopupState(PopupState.BOTTOM);
        mFuseboxPopup.updateLayout();

        assertEquals(50, mFuseboxPopup.mScrollView.getPaddingBottom());
    }

    @Test
    public void testFlingDismissesPopup_whenBottomSheet() {
        recreateFuseboxPopup(/* isBottomSheet= */ true);

        // Call onFling directly on the exposed listener to avoid flaky MotionEvents.
        int minFlingVelocity = ViewConfiguration.get(mActivity).getScaledMinimumFlingVelocity();
        mFuseboxPopup.mScrollView.mGestureListener.onFling(null, null, 0, minFlingVelocity + 1);

        verify(mPopupWindow).dismiss();
    }

    @Test
    public void testObserveDynamicRectProvider_callsUpdateLayout() {
        doReturn(true).when(mPopupWindow).isShowing();
        mFuseboxPopup.setPopupState(PopupState.FLOATING);

        verify(mDynamicRectProvider).startObserving(mObserverCaptor.capture());
        mObserverCaptor.getValue().onRectChanged();

        verify(mPopupWindow, atLeastOnce())
                .updateDesiredContentSize(any(Integer.class), eq(0), eq(true));
    }

    @Test
    public void testLayoutDirection_Rtl() {
        LocalizationUtils.setRtlForTesting(true);
        mActivity.getApplicationInfo().flags |= ApplicationInfo.FLAG_SUPPORTS_RTL;
        ContextUtils.getApplicationContext().getApplicationInfo().flags |=
                ApplicationInfo.FLAG_SUPPORTS_RTL;

        Configuration config = new Configuration(mActivity.getResources().getConfiguration());
        config.setLayoutDirection(new Locale("ar"));
        mActivity.getResources().updateConfiguration(config, null);
        ResettersForTesting.register(
                () -> {
                    config.setLayoutDirection(Locale.getDefault());
                    mActivity.getResources().updateConfiguration(config, null);
                });

        recreateFuseboxPopup(/* isBottomSheet= */ false);

        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(View.LAYOUT_DIRECTION_RTL, mFuseboxPopup.mScrollView.getLayoutDirection());
    }

    @Test
    public void testLayoutDirection_Ltr() {
        LocalizationUtils.setRtlForTesting(false);
        mActivity.getApplicationInfo().flags |= ApplicationInfo.FLAG_SUPPORTS_RTL;
        ContextUtils.getApplicationContext().getApplicationInfo().flags |=
                ApplicationInfo.FLAG_SUPPORTS_RTL;

        Configuration config = new Configuration(mActivity.getResources().getConfiguration());
        config.setLayoutDirection(Locale.getDefault());
        mActivity.getResources().updateConfiguration(config, null);

        recreateFuseboxPopup(/* isBottomSheet= */ false);

        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(View.LAYOUT_DIRECTION_LTR, mFuseboxPopup.mScrollView.getLayoutDirection());
    }

    @Test
    public void testUpdateInsets_BottomSheet_MultiWindow_Top() {
        recreateFuseboxPopup(/* isBottomSheet= */ true);
        mFuseboxPopup.setPopupState(PopupState.BOTTOM);
        doReturn(true).when(mPopupWindow).isShowing();

        Shadows.shadowOf(mActivity).setInMultiWindowMode(true);
        setupMultiWindowMetrics(new Rect(0, 0, 800, 1000), new Rect(0, 0, 800, 2000), 100);

        int initialPadding = mFuseboxPopup.mScrollView.getPaddingBottom();
        mFuseboxPopup.updateLayout();

        assertEquals(initialPadding, mFuseboxPopup.mScrollView.getPaddingBottom());
    }

    @Test
    public void testUpdateInsets_BottomSheet_MultiWindow_Bottom() {
        recreateFuseboxPopup(/* isBottomSheet= */ true);
        mFuseboxPopup.setPopupState(PopupState.BOTTOM);
        doReturn(true).when(mPopupWindow).isShowing();

        Shadows.shadowOf(mActivity).setInMultiWindowMode(true);
        setupMultiWindowMetrics(new Rect(0, 1000, 800, 1900), new Rect(0, 0, 800, 2000), 100);

        int initialPadding = mFuseboxPopup.mScrollView.getPaddingBottom();
        mFuseboxPopup.updateLayout();

        assertEquals(initialPadding + 100, mFuseboxPopup.mScrollView.getPaddingBottom());
    }
}
