// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.toolbar.MiniOriginBarController.MiniOriginWindowInsetsAnimationListener;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.ui.InsetObserver;

import java.util.Collections;
import java.util.function.BooleanSupplier;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniOriginBarControllerTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();

    @Mock private ControlContainer mControlContainer;
    @Mock private LocationBar mLocationBar;
    @Mock private ViewGroup mLocationBarView;
    @Mock private BrowserControlsSizer mBrowserControlsSizer;
    @Mock private InsetObserver mInsetObserver;
    @Captor ArgumentCaptor<TouchEventObserver> mTouchEventObserverCaptor;
    @Captor private ArgumentCaptor<FrameLayout.LayoutParams> mLayoutParamsCaptor;

    private Context mContext;
    private final CoordinatorLayout.LayoutParams mControlContainerLayoutParams =
            new LayoutParams(400, 800);
    private final FrameLayout.LayoutParams mLocationBarLayoutParams =
            new FrameLayout.LayoutParams(400, 800, Gravity.TOP);
    private final FormFieldFocusedSupplier mIsFormFieldFocused = new FormFieldFocusedSupplier();
    private final ToolbarPositionControllerTest.FakeKeyboardVisibilityDelegate
            mKeyboardVisibilityDelegate =
                    new ToolbarPositionControllerTest.FakeKeyboardVisibilityDelegate();
    private MiniOriginBarController mMiniOriginBarController;
    private final ObservableSupplierImpl<Boolean> mSuppressToolbarSceneLayerSupplier =
            new ObservableSupplierImpl<>(false);
    ObservableSupplierImpl<Integer> mControlContainerTranslationSupplier =
            new ObservableSupplierImpl<>(0);
    private boolean mIsSheetShowing;
    BooleanSupplier mIsKeyboardAccessorySheetShowing = () -> mIsSheetShowing;

    private final WindowInsetsAnimationCompat mImeAnimation =
            new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
    private final WindowInsetsAnimationCompat mNonImeAnimation =
            new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.systemBars(), null, 160);
    private boolean mAnimationStarted;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mControlContainerLayoutParams.gravity = Gravity.TOP;
        doReturn(ControlsPosition.TOP).when(mBrowserControlsSizer).getControlsPosition();
        doReturn(mControlContainerLayoutParams).when(mControlContainer).mutateLayoutParams();
        doReturn(mLocationBarView).when(mLocationBar).getContainerView();
        doReturn(mLocationBarLayoutParams).when(mLocationBarView).getLayoutParams();
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
                        mIsKeyboardAccessorySheetShowing);
    }

    @Test
    public void testUpdateMiniOriginBarState() {
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        verify(mLocationBar, never()).setShowOriginOnly(anyBoolean());

        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        verify(mLocationBar, never()).setShowOriginOnly(anyBoolean());

        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        verify(mLocationBar).setShowOriginOnly(true);
        verify(mLocationBar).setUrlBarUsesSmallText(true);
        verify(mLocationBarView).setLayoutParams(mLayoutParamsCaptor.capture());
        assertEquals(Gravity.CENTER, mLayoutParamsCaptor.getValue().gravity);
        assertEquals(ViewGroup.LayoutParams.WRAP_CONTENT, mLayoutParamsCaptor.getValue().width);
        assertEquals(
                mContext.getResources().getDimensionPixelSize(R.dimen.mini_origin_bar_height),
                mLayoutParamsCaptor.getValue().height);

        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        verify(mLocationBar).setShowOriginOnly(false);
        verify(mLocationBar).setUrlBarUsesSmallText(false);
        assertEquals(LayoutParams.WRAP_CONTENT, mControlContainerLayoutParams.height);
        assertEquals(Gravity.TOP, mLocationBarLayoutParams.gravity);
    }

    @Test
    public void testDestroy() {
        mMiniOriginBarController.destroy();
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);

        verify(mLocationBar, never()).setShowOriginOnly(true);
    }

    @Test
    public void testTouchEventEndsMiniOriginModeForSession() {
        verify(mControlContainer).addTouchEventObserver(mTouchEventObserverCaptor.capture());
        MotionEvent clickEvent = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);

        TouchEventObserver observer = mTouchEventObserverCaptor.getValue();
        assertFalse(observer.onInterceptTouchEvent(clickEvent));

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsSizer).getControlsPosition();
        mMiniOriginBarController.onControlsPositionChanged(ControlsPosition.BOTTOM);
        verify(mLocationBar).setShowOriginOnly(true);

        assertTrue(observer.onInterceptTouchEvent(clickEvent));

        verify(mLocationBar).setShowOriginOnly(false);
        verify(mLocationBar).setUrlBarUsesSmallText(false);
        assertFalse(observer.onInterceptTouchEvent(clickEvent));

        mIsFormFieldFocused.onNodeAttributeUpdated(false, false);
        // The effect of the click only persists until the "session" ends, e.g. via un-focusing a
        // form or hiding the keyboard.
        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        assertTrue(observer.onInterceptTouchEvent(clickEvent));
    }

    @Test
    public void testAnimateWithKeyboard_notReadyForAnimation() {
        final MiniOriginWindowInsetsAnimationListener animationListener =
                new MiniOriginWindowInsetsAnimationListener(
                        mKeyboardVisibilityDelegate,
                        mLocationBarView,
                        mControlContainerTranslationSupplier,
                        () -> mAnimationStarted = true,
                        mMiniOriginBarController::waitingForImeAnimationToStart);
        final BoundsCompat bounds = new BoundsCompat(Insets.NONE, Insets.of(0, 0, 0, 40));

        animationListener.onStart(mImeAnimation, bounds);
        assertFalse(mAnimationStarted);

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        animationListener.onStart(mImeAnimation, bounds);
        assertTrue(mAnimationStarted);

        animationListener.onEnd(mImeAnimation);
        mAnimationStarted = false;
        mIsSheetShowing = true;
        animationListener.onStart(mImeAnimation, bounds);
        assertFalse(mAnimationStarted);

        mIsSheetShowing = false;
        animationListener.onStart(mNonImeAnimation, bounds);
        assertFalse(mAnimationStarted);
    }

    @Test
    public void testAnimateWithKeyboard() {
        final MiniOriginWindowInsetsAnimationListener animationListener =
                new MiniOriginWindowInsetsAnimationListener(
                        mKeyboardVisibilityDelegate,
                        mLocationBarView,
                        mControlContainerTranslationSupplier,
                        () -> mAnimationStarted = true,
                        mMiniOriginBarController::waitingForImeAnimationToStart);
        final int finalKeyboardHeight = 100;
        final BoundsCompat bounds =
                new BoundsCompat(Insets.NONE, Insets.of(0, 0, 0, finalKeyboardHeight));

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);

        animationListener.onStart(mImeAnimation, bounds);
        assertTrue(mAnimationStarted);

        int currentKeyboardHeight = 10;
        WindowInsetsCompat insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.ime(),
                                Insets.of(0, 0, 0, currentKeyboardHeight))
                        .build();
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));
        assertEquals(
                finalKeyboardHeight - currentKeyboardHeight,
                (int) mControlContainerTranslationSupplier.get());

        currentKeyboardHeight = 40;
        insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.ime(),
                                Insets.of(0, 0, 0, currentKeyboardHeight))
                        .build();
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));
        assertEquals(
                finalKeyboardHeight - currentKeyboardHeight,
                (int) mControlContainerTranslationSupplier.get());

        currentKeyboardHeight = 90;
        insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.ime(),
                                Insets.of(0, 0, 0, currentKeyboardHeight))
                        .build();
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));
        assertEquals(
                finalKeyboardHeight - currentKeyboardHeight,
                (int) mControlContainerTranslationSupplier.get());

        animationListener.onEnd(mImeAnimation);
        assertEquals(0, (int) mControlContainerTranslationSupplier.get());

        // Simulate hiding the keyboard
        mIsFormFieldFocused.onNodeAttributeUpdated(false, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        mAnimationStarted = false;

        animationListener.onStart(mImeAnimation, bounds);
        assertTrue(mAnimationStarted);

        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));
        assertEquals(-currentKeyboardHeight, (int) mControlContainerTranslationSupplier.get());

        currentKeyboardHeight = 40;
        insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.ime(),
                                Insets.of(0, 0, 0, currentKeyboardHeight))
                        .build();
        animationListener.onProgress(insets, Collections.singletonList(mImeAnimation));
        assertEquals(-currentKeyboardHeight, (int) mControlContainerTranslationSupplier.get());

        animationListener.onEnd(mImeAnimation);
        assertEquals(0, (int) mControlContainerTranslationSupplier.get());
    }

    @Test
    public void testAnimateWithKeyboard_animationFinishesInStartingState() {
        // Predictive back gestures can cause an IME hide animation to run but finish with the IME
        // still showing if the gesture is cancelled.

        final MiniOriginWindowInsetsAnimationListener animationListener =
                new MiniOriginWindowInsetsAnimationListener(
                        mKeyboardVisibilityDelegate,
                        mLocationBarView,
                        mControlContainerTranslationSupplier,
                        () -> mAnimationStarted = true,
                        mMiniOriginBarController::waitingForImeAnimationToStart);
        final int finalKeyboardHeight = 100;
        final BoundsCompat bounds =
                new BoundsCompat(Insets.NONE, Insets.of(0, 0, 0, finalKeyboardHeight));

        mIsFormFieldFocused.onNodeAttributeUpdated(true, false);
        mKeyboardVisibilityDelegate.setVisibilityForTests(true);

        animationListener.onStart(mImeAnimation, bounds);
        animationListener.onEnd(mImeAnimation);
        assertEquals(0, (int) mControlContainerTranslationSupplier.get());

        // Simulate the beginning of an animation hiding the keyboard

        mKeyboardVisibilityDelegate.setVisibilityForTests(false);
        mAnimationStarted = false;

        animationListener.onStart(mImeAnimation, bounds);
        assertTrue(mAnimationStarted);

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
    }

    // show again, start, finish showing (predictive back)
}
