// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.toolbar.MiniOriginBarController.MiniOriginState;
import org.chromium.chrome.browser.toolbar.MiniOriginBarController.MiniOriginWindowInsetsAnimationListener;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.ui.insets.InsetObserver;

import java.util.Collections;
import java.util.function.BooleanSupplier;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniOriginBarControllerTest {

    private static final int CONTROL_CONTAINER_WIDTH = 400;
    private static final int FULL_TOOLBAR_HEIGHT = 56;
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();

    @Mock private ControlContainer mControlContainer;
    @Mock private LocationBar mLocationBar;
    @Mock private ViewGroup mLocationBarView;
    @Mock private View mControlContainerView;
    @Mock private BrowserControlsSizer mBrowserControlsSizer;
    @Mock private InsetObserver mInsetObserver;
    @Mock private WebContentsImpl mWebContents;
    @Mock private ImeAdapterImpl mImeAdapter;
    @Captor ArgumentCaptor<TouchEventObserver> mTouchEventObserverCaptor;
    @Captor private ArgumentCaptor<CoordinatorLayout.LayoutParams> mLayoutParamsCaptor;

    private Context mContext;
    private final CoordinatorLayout.LayoutParams mControlContainerLayoutParams =
            new LayoutParams(CONTROL_CONTAINER_WIDTH, 120);
    private final FrameLayout.LayoutParams mLocationBarLayoutParams =
            new FrameLayout.LayoutParams(200, 100, Gravity.TOP);
    private final FormFieldFocusedSupplier mIsFormFieldFocused = new FormFieldFocusedSupplier();
    private final ToolbarPositionControllerTest.FakeKeyboardVisibilityDelegate
            mKeyboardVisibilityDelegate =
                    new ToolbarPositionControllerTest.FakeKeyboardVisibilityDelegate();
    private MiniOriginBarController mMiniOriginBarController;
    private final ObservableSupplierImpl<Boolean> mSuppressToolbarSceneLayerSupplier =
            new ObservableSupplierImpl<>(false);
    ObservableSupplierImpl<Integer> mControlContainerTranslationSupplier =
            new ObservableSupplierImpl<>(0);

    private final ObservableSupplierImpl<Boolean> mIsKeyboardAccessorySheetShowing =
            new ObservableSupplierImpl<>(false);

    private final WindowInsetsAnimationCompat mImeAnimation =
            new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
    private final WindowInsetsAnimationCompat mNonImeAnimation =
            new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.systemBars(), null, 160);
    private boolean mOmniboxFocused;
    private final BooleanSupplier mIsOmniboxFocusedSupplier = () -> mOmniboxFocused;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mControlContainerLayoutParams.gravity = Gravity.TOP;
        doReturn(ControlsPosition.TOP).when(mBrowserControlsSizer).getControlsPosition();
        doReturn(mControlContainerLayoutParams).when(mControlContainer).mutateLayoutParams();
        doReturn(mLocationBarView).when(mLocationBar).getContainerView();
        doReturn(mLocationBarLayoutParams).when(mLocationBarView).getLayoutParams();
        doReturn(mControlContainerView).when(mControlContainer).getView();
        doReturn(FULL_TOOLBAR_HEIGHT).when(mControlContainer).getToolbarHeight();
        doReturn(mControlContainerLayoutParams.width).when(mControlContainerView).getWidth();
        doReturn(mImeAdapter).when(mWebContents).getOrSetUserData(eq(ImeAdapterImpl.class), any());
        mIsFormFieldFocused.onWebContentsChanged(mWebContents);
        mMiniOriginBarController =
                new MiniOriginBarController(
                        mLocationBar,
                        mIsFormFieldFocused,
                        mKeyboardVisibilityDelegate,
                        mContext,
                        mControlContainer,
                        mSuppressToolbarSceneLayerSupplier,
                        mBrowserControlsSizer,
                        mInsetObserver,
                        mControlContainerTranslationSupplier,
                        mIsKeyboardAccessorySheetShowing,
                        mIsOmniboxFocusedSupplier);
    }

    @Test
    public void testUpdateMiniOriginBarState() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        verify(mLocationBar, never()).setShowOriginOnly(anyBoolean());

        mKeyboardVisibilityDelegate.setVisibilityForTests(true);

        verify(mLocationBar).setShowOriginOnly(true);
        verify(mLocationBar).setUrlBarUsesSmallText(true);
        verify(mLocationBarView).setLayoutParams(mLayoutParamsCaptor.capture());
        assertEquals(Gravity.CENTER_VERTICAL, mLayoutParamsCaptor.getValue().gravity);
        assertEquals(ViewGroup.LayoutParams.WRAP_CONTENT, mLayoutParamsCaptor.getValue().width);

        final int miniOriginBarHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.mini_origin_bar_height);
        final int hairlineHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        assertEquals(miniOriginBarHeight, mLayoutParamsCaptor.getValue().height);
        assertEquals(miniOriginBarHeight + hairlineHeight, mControlContainerLayoutParams.height);
        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());

        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        verify(mLocationBar).setShowOriginOnly(false);
        verify(mLocationBar).setUrlBarUsesSmallText(false);
        assertEquals(LayoutParams.WRAP_CONTENT, mControlContainerLayoutParams.height);
        assertEquals(Gravity.TOP, mLocationBarLayoutParams.gravity);
        Assert.assertEquals(
                MiniOriginState.READY, mMiniOriginBarController.getCurrentStateForTesting());
    }

    @Test
    public void testFormFieldFocusLostFromShowing() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);

        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());

        mIsFormFieldFocused.onNodeAttributeUpdated(false, false);
        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());

        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        Assert.assertEquals(
                MiniOriginState.NOT_READY, mMiniOriginBarController.getCurrentStateForTesting());
    }

    @Test
    public void testDestroy() {
        mMiniOriginBarController.destroy();
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);

        verify(mLocationBar, never()).setShowOriginOnly(true);
    }

    @Test
    public void testTouchEventHidesKeyboard() {
        verify(mControlContainer).addTouchEventObserver(mTouchEventObserverCaptor.capture());
        MotionEvent clickEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);

        TouchEventObserver observer = mTouchEventObserverCaptor.getValue();
        assertFalse(observer.onInterceptTouchEvent(clickEvent));

        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        verify(mLocationBar).setShowOriginOnly(true);
        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());

        assertTrue(observer.onInterceptTouchEvent(clickEvent));
        verify(mImeAdapter).resetAndHideKeyboard();
    }

    @Test
    public void testFormFieldFocusWithOmniboxFocused() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        mOmniboxFocused = true;
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        Assert.assertEquals(
                MiniOriginState.NOT_READY, mMiniOriginBarController.getCurrentStateForTesting());
    }

    @Test
    public void testAnimateWithKeyboard_notReadyForAnimation() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        final MiniOriginWindowInsetsAnimationListener animationListener =
                mMiniOriginBarController.getAnimationListenerForTesting();
        final BoundsCompat bounds = new BoundsCompat(Insets.NONE, Insets.of(0, 0, 0, 40));

        animationListener.onPrepare(mImeAnimation);
        animationListener.onStart(mImeAnimation, bounds);
        Assert.assertEquals(
                MiniOriginState.NOT_READY, mMiniOriginBarController.getCurrentStateForTesting());

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        animationListener.onPrepare(mImeAnimation);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        animationListener.onStart(mImeAnimation, bounds);
        Assert.assertEquals(
                MiniOriginState.ANIMATING, mMiniOriginBarController.getCurrentStateForTesting());

        animationListener.onEnd(mImeAnimation);
        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());
        mIsKeyboardAccessorySheetShowing.set(true);
        Assert.assertEquals(
                MiniOriginState.SHOWING_WITH_ACCESSORY_SHEET,
                mMiniOriginBarController.getCurrentStateForTesting());

        animationListener.onPrepare(mImeAnimation);
        Assert.assertEquals(
                MiniOriginState.SHOWING_WITH_ACCESSORY_SHEET,
                mMiniOriginBarController.getCurrentStateForTesting());

        mIsKeyboardAccessorySheetShowing.set(false);
        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());
    }

    @Test
    public void testSkipAnimation() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);

        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());
        verify(mLocationBarView).setScaleX(MiniOriginBarController.LOCATION_BAR_FINAL_SCALE);
        verify(mLocationBarView).setScaleY(MiniOriginBarController.LOCATION_BAR_FINAL_SCALE);
    }

    @Test
    public void testAnimateWithKeyboard() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);

        final MiniOriginWindowInsetsAnimationListener animationListener =
                mMiniOriginBarController.getAnimationListenerForTesting();
        final int finalKeyboardHeight = 100;
        final BoundsCompat bounds =
                new BoundsCompat(Insets.NONE, Insets.of(0, 0, 0, finalKeyboardHeight));

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);

        final int locationBarStartPosition = 50;
        mLocationBarLayoutParams.leftMargin = locationBarStartPosition;
        final int locationBarMiniWidth = 100;
        doReturn(locationBarMiniWidth).when(mLocationBarView).getMeasuredWidth();
        float finalLocationBarWidth =
                locationBarMiniWidth * MiniOriginBarController.LOCATION_BAR_FINAL_SCALE;
        final float finalX = (CONTROL_CONTAINER_WIDTH - finalLocationBarWidth) / 2;
        final float positionDelta = finalX - locationBarStartPosition;
        // The url bar height is smaller than the total height of the mobar due to vertical
        // margin.
        final float urlBarHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.mini_origin_bar_height) - 6;
        doReturn(urlBarHeight).when(mLocationBar).getUrlBarHeight();

        animationListener.onPrepare(mImeAnimation);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        animationListener.onStart(mImeAnimation, bounds);
        Assert.assertEquals(
                MiniOriginState.ANIMATING, mMiniOriginBarController.getCurrentStateForTesting());

        int currentKeyboardHeight = 10;
        final int systemBarsHeight = 13;
        WindowInsetsCompat.Builder insetsBuilder =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.ime(),
                                Insets.of(0, 0, 0, currentKeyboardHeight))
                        .setInsets(
                                WindowInsetsCompat.Type.systemBars(),
                                Insets.of(0, 0, 0, systemBarsHeight));
        WindowInsetsCompat insets = insetsBuilder.build();

        mImeAnimation.setFraction(0.1f);
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));
        assertEquals(
                finalKeyboardHeight - currentKeyboardHeight,
                (int) mControlContainerTranslationSupplier.get());
        verify(mLocationBarView)
                .setTranslationX(
                        locationBarStartPosition + mImeAnimation.getFraction() * positionDelta);
        verify(mLocationBarView)
                .setScaleX(
                        1.0f
                                - mImeAnimation.getFraction()
                                        / MiniOriginBarController.LOCATION_BAR_SCALE_DENOMINATOR);
        verify(mLocationBarView)
                .setScaleY(
                        1.0f
                                - mImeAnimation.getFraction()
                                        / MiniOriginBarController.LOCATION_BAR_SCALE_DENOMINATOR);
        verify(mLocationBarView).setPivotY(urlBarHeight / 2);
        verify(mLocationBarView).setPivotX(0.0f);

        currentKeyboardHeight = 40;
        insets =
                insetsBuilder
                        .setInsets(
                                WindowInsetsCompat.Type.ime(),
                                Insets.of(0, 0, 0, currentKeyboardHeight))
                        .build();
        mImeAnimation.setFraction(0.4f);
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));
        assertEquals(
                finalKeyboardHeight - currentKeyboardHeight,
                (int) mControlContainerTranslationSupplier.get());
        verify(mLocationBarView)
                .setTranslationX(
                        locationBarStartPosition + mImeAnimation.getFraction() * positionDelta);
        verify(mLocationBarView)
                .setScaleX(
                        1.0f
                                - mImeAnimation.getFraction()
                                        / MiniOriginBarController.LOCATION_BAR_SCALE_DENOMINATOR);
        verify(mLocationBarView)
                .setScaleY(
                        1.0f
                                - mImeAnimation.getFraction()
                                        / MiniOriginBarController.LOCATION_BAR_SCALE_DENOMINATOR);

        currentKeyboardHeight = 90;
        insets =
                insetsBuilder
                        .setInsets(
                                WindowInsetsCompat.Type.ime(),
                                Insets.of(0, 0, 0, currentKeyboardHeight))
                        .build();
        mImeAnimation.setFraction(0.9f);
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));
        assertEquals(
                finalKeyboardHeight - currentKeyboardHeight,
                (int) mControlContainerTranslationSupplier.get());
        verify(mLocationBarView)
                .setTranslationX(
                        locationBarStartPosition + mImeAnimation.getFraction() * positionDelta);
        verify(mLocationBarView)
                .setScaleX(
                        1.0f
                                - mImeAnimation.getFraction()
                                        / MiniOriginBarController.LOCATION_BAR_SCALE_DENOMINATOR);
        verify(mLocationBarView)
                .setScaleY(
                        1.0f
                                - mImeAnimation.getFraction()
                                        / MiniOriginBarController.LOCATION_BAR_SCALE_DENOMINATOR);

        animationListener.onEnd(mImeAnimation);
        assertEquals(0, (int) mControlContainerTranslationSupplier.get());
        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());
        verify(mLocationBarView).setTranslationX(locationBarStartPosition + positionDelta);
        verify(mLocationBarView).setScaleX(MiniOriginBarController.LOCATION_BAR_FINAL_SCALE);
        verify(mLocationBarView).setScaleY(MiniOriginBarController.LOCATION_BAR_FINAL_SCALE);

        Mockito.clearInvocations(mLocationBarView);
        // Simulate hiding the keyboard
        mKeyboardVisibilityDelegate.setVisibilityForTests(false);

        animationListener.onPrepare(mImeAnimation);
        animationListener.onStart(mImeAnimation, bounds);
        Assert.assertEquals(
                MiniOriginState.ANIMATING, mMiniOriginBarController.getCurrentStateForTesting());
        mImeAnimation.setFraction(0.1f);
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));
        assertEquals(
                -currentKeyboardHeight + systemBarsHeight,
                (int) mControlContainerTranslationSupplier.get());
        verify(mLocationBarView)
                .setTranslationX(
                        locationBarStartPosition
                                + (1.0f - mImeAnimation.getFraction()) * positionDelta);
        verify(mLocationBarView)
                .setScaleX(
                        1.0f
                                - (1.0f - mImeAnimation.getFraction())
                                        / MiniOriginBarController.LOCATION_BAR_SCALE_DENOMINATOR);
        verify(mLocationBarView)
                .setScaleY(
                        1.0f
                                - (1.0f - mImeAnimation.getFraction())
                                        / MiniOriginBarController.LOCATION_BAR_SCALE_DENOMINATOR);

        currentKeyboardHeight = 45;
        insets =
                insetsBuilder
                        .setInsets(
                                WindowInsetsCompat.Type.ime(),
                                Insets.of(0, 0, 0, currentKeyboardHeight))
                        .build();
        mImeAnimation.setFraction(0.6f);
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));
        assertEquals(
                -currentKeyboardHeight + systemBarsHeight,
                (int) mControlContainerTranslationSupplier.get());
        verify(mLocationBarView)
                .setTranslationX(
                        locationBarStartPosition
                                + (1.0f - mImeAnimation.getFraction()) * positionDelta);
        verify(mLocationBarView)
                .setScaleX(
                        1.0f
                                - (1.0f - mImeAnimation.getFraction())
                                        / MiniOriginBarController.LOCATION_BAR_SCALE_DENOMINATOR);
        verify(mLocationBarView)
                .setScaleY(
                        1.0f
                                - (1.0f - mImeAnimation.getFraction())
                                        / MiniOriginBarController.LOCATION_BAR_SCALE_DENOMINATOR);

        Mockito.clearInvocations(mLocationBarView);
        animationListener.onEnd(mImeAnimation);
        Assert.assertEquals(
                MiniOriginState.READY, mMiniOriginBarController.getCurrentStateForTesting());
        assertEquals(0, (int) mControlContainerTranslationSupplier.get());
        verify(mLocationBarView).setTranslationX(locationBarStartPosition);
        verify(mLocationBarView).setScaleX(1.0f);
        verify(mLocationBarView).setScaleY(1.0f);
    }

    @Test
    public void testAnimateWithKeyboard_animationWithZeroMaxHeight() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        final MiniOriginWindowInsetsAnimationListener animationListener =
                mMiniOriginBarController.getAnimationListenerForTesting();

        final BoundsCompat bounds = new BoundsCompat(Insets.NONE, Insets.of(0, 0, 0, 0));

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);

        animationListener.onPrepare(mImeAnimation);
        animationListener.onStart(mImeAnimation, bounds);
        Assert.assertEquals(
                MiniOriginState.READY, mMiniOriginBarController.getCurrentStateForTesting());
    }

    @Test
    public void testLoseFormFocusDuringAnimation() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        final MiniOriginWindowInsetsAnimationListener animationListener =
                mMiniOriginBarController.getAnimationListenerForTesting();
        final BoundsCompat bounds = new BoundsCompat(Insets.NONE, Insets.of(0, 0, 0, 100));

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);

        animationListener.onPrepare(mImeAnimation);
        animationListener.onStart(mImeAnimation, bounds);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        Assert.assertEquals(
                MiniOriginState.ANIMATING, mMiniOriginBarController.getCurrentStateForTesting());

        mIsFormFieldFocused.onNodeAttributeUpdated(false, false);
        Assert.assertEquals(
                MiniOriginState.NOT_READY, mMiniOriginBarController.getCurrentStateForTesting());
    }

    @Test
    public void testAnimateWithKeyboard_animationFinishesInStartingState() {
        // Predictive back gestures can cause an IME hide animation to run but finish with the IME
        // still showing if the gesture is cancelled.

        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        final MiniOriginWindowInsetsAnimationListener animationListener =
                mMiniOriginBarController.getAnimationListenerForTesting();

        final int finalKeyboardHeight = 100;
        final BoundsCompat bounds =
                new BoundsCompat(Insets.NONE, Insets.of(0, 0, 0, finalKeyboardHeight));

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);

        animationListener.onPrepare(mImeAnimation);
        animationListener.onStart(mImeAnimation, bounds);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        Assert.assertEquals(
                MiniOriginState.ANIMATING, mMiniOriginBarController.getCurrentStateForTesting());

        animationListener.onEnd(mImeAnimation);
        assertEquals(0, (int) mControlContainerTranslationSupplier.get());
        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());

        // Simulate the beginning of an animation hiding the keyboard

        animationListener.onPrepare(mImeAnimation);
        Assert.assertEquals(
                MiniOriginState.ANIMATING, mMiniOriginBarController.getCurrentStateForTesting());

        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        animationListener.onStart(mImeAnimation, bounds);

        int currentKeyboardHeight = 90;
        WindowInsetsCompat insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.ime(),
                                Insets.of(0, 0, 0, currentKeyboardHeight))
                        .build();
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));
        assertEquals(-currentKeyboardHeight, (int) mControlContainerTranslationSupplier.get());

        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        animationListener.onEnd(mImeAnimation);
        assertEquals(0, (int) mControlContainerTranslationSupplier.get());
        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());
    }

    @Test
    public void testAnimateWithKeyboard_endEarly() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        final MiniOriginWindowInsetsAnimationListener animationListener =
                mMiniOriginBarController.getAnimationListenerForTesting();

        final int finalKeyboardHeight = 100;
        final BoundsCompat bounds =
                new BoundsCompat(Insets.NONE, Insets.of(0, 0, 0, finalKeyboardHeight));
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);

        animationListener.onPrepare(mImeAnimation);
        animationListener.onStart(mImeAnimation, bounds);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);

        animationListener.onEnd(mImeAnimation);
        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());

        // Simulate the beginning of an animation hiding the keyboard
        animationListener.onPrepare(mImeAnimation);
        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        animationListener.onStart(mImeAnimation, bounds);

        final int miniOriginBarHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.mini_origin_bar_height);
        int currentKeyboardHeight = FULL_TOOLBAR_HEIGHT - miniOriginBarHeight;
        WindowInsetsCompat insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.ime(),
                                Insets.of(0, 0, 0, currentKeyboardHeight))
                        .build();
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));

        Assert.assertEquals(
                MiniOriginState.READY, mMiniOriginBarController.getCurrentStateForTesting());
        assertEquals(0, (int) mControlContainerTranslationSupplier.get());
        verify(mLocationBarView).setScaleX(1.0f);
        verify(mLocationBarView).setScaleY(1.0f);
    }

    @Test
    public void testOmniboxFocused() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mOmniboxFocused = true;

        final MiniOriginWindowInsetsAnimationListener animationListener =
                mMiniOriginBarController.getAnimationListenerForTesting();

        animationListener.onPrepare(mImeAnimation);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        verify(mLocationBar, never()).setShowOriginOnly(anyBoolean());
    }

    @Test
    public void testOmniboxFocusCausesPositionChangeToBottom() {
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        mOmniboxFocused = true;
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);

        Assert.assertEquals(
                MiniOriginState.NOT_READY, mMiniOriginBarController.getCurrentStateForTesting());
    }

    @Test
    public void testAnimationReplacement() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        final MiniOriginWindowInsetsAnimationListener animationListener =
                mMiniOriginBarController.getAnimationListenerForTesting();

        final int finalKeyboardHeight = 100;
        final BoundsCompat bounds =
                new BoundsCompat(Insets.NONE, Insets.of(0, 0, 0, finalKeyboardHeight));
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);

        animationListener.onPrepare(mImeAnimation);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        animationListener.onStart(mImeAnimation, bounds);

        int currentKeyboardHeight = 50;
        WindowInsetsCompat insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.ime(),
                                Insets.of(0, 0, 0, currentKeyboardHeight))
                        .build();
        mImeAnimation.setFraction(0.5f);
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));

        // Start a second animation, this time hiding the keyboard.
        final WindowInsetsAnimationCompat secondImeAnimation =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        animationListener.onStart(secondImeAnimation, bounds);

        secondImeAnimation.setFraction(0.5f);
        animationListener.onProgress(insets, Collections.singletonList(secondImeAnimation));
        assertEquals(-currentKeyboardHeight, (int) mControlContainerTranslationSupplier.get());
        assertTrue(mSuppressToolbarSceneLayerSupplier.get());

        animationListener.onEnd(secondImeAnimation);
        assertFalse(mSuppressToolbarSceneLayerSupplier.get());
    }

    @Test
    public void testAnimationCancel() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        final MiniOriginWindowInsetsAnimationListener animationListener =
                mMiniOriginBarController.getAnimationListenerForTesting();

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);

        animationListener.onPrepare(mImeAnimation);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertEquals(
                MiniOriginState.SHOWING, mMiniOriginBarController.getCurrentStateForTesting());
    }
}
