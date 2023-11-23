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

import android.graphics.Rect;
import android.os.Build;
import android.view.WindowInsets;
import android.widget.LinearLayout;

import androidx.annotation.RequiresApi;
import androidx.core.graphics.Insets;
import androidx.core.view.DisplayCutoutCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.components.browser_ui.widget.InsetObserver.WindowInsetsConsumer;

import java.util.Collections;

/** Tests for {@link InsetObserver} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InsetObserverTest {
    /** The rect values if the display cutout is present. */
    private static final Rect DISPLAY_CUTOUT_RECT = new Rect(1, 1, 1, 1);

    /** The rect values if there is no cutout. */
    private static final Rect NO_CUTOUT_RECT = new Rect(0, 0, 0, 0);

    @Mock private InsetObserver.WindowInsetObserver mObserver;

    @Mock private WindowInsetsCompat mInsets;
    @Mock private WindowInsetsCompat mModifiedInsets;
    @Mock private WindowInsets mNonCompatInsets;
    @Mock private WindowInsets mModifiedNonCompatInsets;
    @Mock private WindowInsetsConsumer mInsetsConsumer;
    @Mock private WindowInsetsAnimationListener mInsetsAnimationListener;
    @Mock private LinearLayout mContentView;

    private InsetObserver mInsetObserver;

    private void setCutout(boolean hasCutout) {
        DisplayCutoutCompat cutout =
                hasCutout ? new DisplayCutoutCompat(new Rect(1, 1, 1, 1), null) : null;
        doReturn(cutout).when(mInsets).getDisplayCutout();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mNonCompatInsets).when(mInsets).toWindowInsets();
        doReturn(mModifiedNonCompatInsets).when(mModifiedInsets).toWindowInsets();
        doReturn(WindowInsetsCompat.CONSUMED.toWindowInsets())
                .when(mContentView)
                .onApplyWindowInsets(mNonCompatInsets);
        doReturn(WindowInsetsCompat.CONSUMED.toWindowInsets())
                .when(mContentView)
                .onApplyWindowInsets(mModifiedNonCompatInsets);

        mInsetObserver = new InsetObserver(mContentView);
        mInsetObserver.addObserver(mObserver);
    }

    /** Test that applying new insets notifies observers. */
    @Test
    @SmallTest
    public void applyInsets_NotifiesObservers() {
        doReturn(1).when(mInsets).getSystemWindowInsetLeft();
        doReturn(1).when(mInsets).getSystemWindowInsetTop();
        doReturn(1).when(mInsets).getSystemWindowInsetRight();
        doReturn(1).when(mInsets).getSystemWindowInsetBottom();
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, times(1)).onInsetChanged(1, 1, 1, 1);

        // Apply the insets a second time; the observer should not be notified.
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, times(1)).onInsetChanged(1, 1, 1, 1);

        doReturn(2).when(mInsets).getSystemWindowInsetBottom();
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onInsetChanged(1, 1, 1, 2);
    }

    @Test
    @SmallTest
    public void applyInsets_withInsetConsumer() {
        mInsetObserver.addInsetsConsumer(mInsetsConsumer);

        doReturn(mModifiedInsets).when(mInsetsConsumer).onApplyWindowInsets(mContentView, mInsets);
        doReturn(14).when(mModifiedInsets).getSystemWindowInsetLeft();
        doReturn(17).when(mModifiedInsets).getSystemWindowInsetTop();
        doReturn(31).when(mModifiedInsets).getSystemWindowInsetRight();
        doReturn(43).when(mModifiedInsets).getSystemWindowInsetBottom();

        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mInsetsConsumer).onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, times(1))
                .onInsetChanged(
                        mModifiedInsets.getSystemWindowInsetLeft(),
                        mModifiedInsets.getSystemWindowInsetTop(),
                        mModifiedInsets.getSystemWindowInsetRight(),
                        mModifiedInsets.getSystemWindowInsetBottom());
    }

    @Test
    @SmallTest
    public void insetAnimation() {
        mInsetObserver.addWindowInsetsAnimationListener(mInsetsAnimationListener);
        WindowInsetsAnimationCompat.Callback callback =
                mInsetObserver.getInsetAnimationProxyCallbackForTesting();
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
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, never()).onSafeAreaChanged(any());
    }

    /** Test that applying new insets with a cutout notifies the observer. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_WithCutout() {
        setCutout(true);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onSafeAreaChanged(DISPLAY_CUTOUT_RECT);
    }

    /** Test applying new insets with a cutout and then remove the cutout. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_WithCutout_WithoutCutout() {
        setCutout(true);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onSafeAreaChanged(DISPLAY_CUTOUT_RECT);

        reset(mObserver);
        setCutout(false);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onSafeAreaChanged(NO_CUTOUT_RECT);
    }

    /** Test that applying new insets with a cutout but no observer is a no-op. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_WithCutout_NoListener() {
        setCutout(true);
        mInsetObserver.removeObserver(mObserver);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
    }

    /** Test that applying new insets with no observer is a no-op. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_NoListener() {
        setCutout(false);
        mInsetObserver.removeObserver(mObserver);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
    }
}
