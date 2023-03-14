// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.view.DisplayCutout;
import android.view.WindowInsets;
import android.widget.LinearLayout;

import androidx.annotation.RequiresApi;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.InsetObserverView.WindowInsetsAnimationListener;
import org.chromium.components.browser_ui.widget.InsetObserverView.WindowInsetsConsumer;

import java.util.Collections;

/**
 * Tests for {@link InsetObserverView} class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InsetObserverViewTest {
    /** The rect values if the display cutout is present. */
    private static final Rect DISPLAY_CUTOUT_RECT = new Rect(1, 1, 1, 1);

    /** The rect values if there is no cutout. */
    private static final Rect NO_CUTOUT_RECT = new Rect(0, 0, 0, 0);

    @Mock
    private InsetObserverView.WindowInsetObserver mObserver;

    @Mock
    private WindowInsets mInsets;
    @Mock
    private WindowInsetsConsumer mInsetsConsumer;
    @Mock
    private WindowInsetsAnimationListener mInsetsAnimationListener;

    private Activity mActivity;

    private InsetObserverView mView;

    private LinearLayout mContentView;

    @RequiresApi(Build.VERSION_CODES.P)
    private void setCutout(boolean hasCutout) {
        DisplayCutout cutout = hasCutout ? new DisplayCutout(new Rect(1, 1, 1, 1), null) : null;
        doReturn(cutout).when(mInsets).getDisplayCutout();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mContentView = new LinearLayout(mActivity);
        mActivity.setContentView(mContentView);

        mView = InsetObserverView.create(mActivity);
        mView.addObserver(mObserver);
    }

    /** Test that applying new insets notifies observers. */
    @Test
    @SmallTest
    public void applyInsets_NotifiesObservers() {
        doReturn(1).when(mInsets).getSystemWindowInsetLeft();
        doReturn(1).when(mInsets).getSystemWindowInsetTop();
        doReturn(1).when(mInsets).getSystemWindowInsetRight();
        doReturn(1).when(mInsets).getSystemWindowInsetBottom();
        mView.onApplyWindowInsets(mInsets);
        verify(mObserver, times(1)).onInsetChanged(1, 1, 1, 1);

        // Apply the insets a second time; the observer should not be notified.
        mView.onApplyWindowInsets(mInsets);
        verify(mObserver, times(1)).onInsetChanged(1, 1, 1, 1);

        doReturn(2).when(mInsets).getSystemWindowInsetBottom();
        mView.onApplyWindowInsets(mInsets);
        verify(mObserver).onInsetChanged(1, 1, 1, 2);
    }

    @Test
    @SmallTest
    public void applyInsets_withInsetConsumer() {
        mView.addInsetsConsumer(mInsetsConsumer);

        WindowInsets windowInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.of(0, 0, 0, 84))
                        .setInsets(WindowInsetsCompat.Type.statusBars(), Insets.of(42, 0, 0, 0))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 300))
                        .build()
                        .toWindowInsets();

        WindowInsetsCompat modifiedInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.of(0, 0, 0, 84))
                        .setInsets(WindowInsetsCompat.Type.statusBars(), Insets.of(42, 0, 0, 0))
                        .build();
        doReturn(modifiedInsets)
                .when(mInsetsConsumer)
                .onApplyWindowInsets(
                        mView, WindowInsetsCompat.toWindowInsetsCompat(windowInsets, mView));

        mView.onApplyWindowInsets(windowInsets);
        verify(mInsetsConsumer)
                .onApplyWindowInsets(
                        mView, WindowInsetsCompat.toWindowInsetsCompat(windowInsets, mView));
        verify(mObserver, times(1))
                .onInsetChanged(modifiedInsets.getSystemWindowInsetLeft(),
                        modifiedInsets.getSystemWindowInsetTop(),
                        modifiedInsets.getSystemWindowInsetRight(),
                        modifiedInsets.getSystemWindowInsetBottom());
    }

    @Test
    @SmallTest
    public void insetAnimation() {
        mView.addWindowInsetsAnimationListener(mInsetsAnimationListener);
        WindowInsetsAnimationCompat.Callback callback =
                mView.getInsetAnimationProxyCallbackForTesting();
        WindowInsetsAnimationCompat animationCompat = new WindowInsetsAnimationCompat(8, null, 50);
        callback.onPrepare(animationCompat);
        verify(mInsetsAnimationListener).onPrepare(animationCompat);

        BoundsCompat bounds = new BoundsCompat(Insets.NONE, Insets.of(0, 0, 40, 40));
        callback.onStart(animationCompat, bounds);
        verify(mInsetsAnimationListener).onStart(animationCompat, bounds);

        WindowInsetsCompat insetsCompat = WindowInsetsCompat.CONSUMED;
        callback.onProgress(insetsCompat, Collections.emptyList());
        callback.onProgress(insetsCompat, Collections.emptyList());
        verify(mInsetsAnimationListener, times(2))
                .onProgress(insetsCompat, Collections.emptyList());

        callback.onEnd(animationCompat);
        verify(mInsetsAnimationListener).onEnd(animationCompat);
    }

    /** Test that applying new insets does not notify the observer. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets() {
        setCutout(false);
        mView.onApplyWindowInsets(mInsets);
        verify(mObserver, never()).onSafeAreaChanged(any());
    }

    /** Test that applying new insets with a cutout notifies the observer. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_WithCutout() {
        setCutout(true);
        mView.onApplyWindowInsets(mInsets);
        verify(mObserver).onSafeAreaChanged(DISPLAY_CUTOUT_RECT);
    }

    /** Test applying new insets with a cutout and then remove the cutout. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_WithCutout_WithoutCutout() {
        setCutout(true);
        mView.onApplyWindowInsets(mInsets);
        verify(mObserver).onSafeAreaChanged(DISPLAY_CUTOUT_RECT);

        reset(mObserver);
        setCutout(false);
        mView.onApplyWindowInsets(mInsets);
        verify(mObserver).onSafeAreaChanged(NO_CUTOUT_RECT);
    }

    /** Test that applying new insets with a cutout but no observer is a no-op. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_WithCutout_NoListener() {
        setCutout(true);
        mView.removeObserver(mObserver);
        mView.onApplyWindowInsets(mInsets);
    }

    /** Test that applying new insets with no observer is a no-op. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_NoListener() {
        setCutout(false);
        mView.removeObserver(mObserver);
        mView.onApplyWindowInsets(mInsets);
    }
}
