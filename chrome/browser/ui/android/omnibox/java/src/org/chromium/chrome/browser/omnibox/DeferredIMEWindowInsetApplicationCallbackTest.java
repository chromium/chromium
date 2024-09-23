// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link DeferredIMEWindowInsetApplicationCallback}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DeferredIMEWindowInsetApplicationCallbackTest {
    private static final Insets STATUS_BAR_INSETS = Insets.of(0, 62, 0, 0);
    private static final Insets NAV_BAR_INSETS = Insets.of(0, 0, 0, 84);

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private DeferredIMEWindowInsetApplicationCallback mCallback;
    private WindowInsetsCompat.Builder mBaseWindowInsets;
    private WindowInsetsAnimationCompat mAnimation;
    private WindowInsetsAnimationCompat mAnimation2;

    @Mock private Activity mActivity;
    @Mock private Runnable mUpdateRunnable;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private View mView;
    @Mock private InsetObserver mInsetObserver;

    @Before
    public void setUp() {
        WeakReference<Activity> activityRef = new WeakReference<>(mActivity);
        when(mActivity.isFinishing()).thenReturn(false);
        when(mWindowAndroid.getActivity()).thenReturn(activityRef);
        when(mWindowAndroid.isDestroyed()).thenReturn(false);
        when(mWindowAndroid.getInsetObserver()).thenReturn(mInsetObserver);
        mAnimation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        mAnimation2 = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        mCallback = new DeferredIMEWindowInsetApplicationCallback(mUpdateRunnable);
        mBaseWindowInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), NAV_BAR_INSETS);
    }

    @Test
    public void testAnimatedkeyboardShowHide() {
        mCallback.attach(mWindowAndroid);
        mCallback.onPrepare(mAnimation);

        WindowInsetsCompat windowInsets =
                mBaseWindowInsets
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 384))
                        .build();
        WindowInsetsCompat modifiedInsets = mCallback.onApplyWindowInsets(mView, windowInsets);

        assertEquals(Insets.NONE, modifiedInsets.getInsets(WindowInsetsCompat.Type.ime()));
        verify(mUpdateRunnable, never()).run();

        mCallback.onEnd(mAnimation);
        verify(mUpdateRunnable).run();
        assertEquals(300, mCallback.getCurrentKeyboardHeight());

        mCallback.onPrepare(mAnimation);
        windowInsets =
                mBaseWindowInsets.setInsets(WindowInsetsCompat.Type.ime(), Insets.NONE).build();
        mCallback.onApplyWindowInsets(mView, windowInsets);
        verify(mUpdateRunnable, times(2)).run();
        assertEquals(0, mCallback.getCurrentKeyboardHeight());

        mCallback.onEnd(mAnimation);
        assertEquals(0, mCallback.getCurrentKeyboardHeight());
    }

    @Test
    public void testAnimatedkeyboardShow_noNavBar() {
        mCallback.attach(mWindowAndroid);
        mCallback.onPrepare(mAnimation);

        WindowInsetsCompat windowInsets =
                mBaseWindowInsets
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE)
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 384))
                        .build();
        WindowInsetsCompat modifiedInsets = mCallback.onApplyWindowInsets(mView, windowInsets);

        assertEquals(Insets.NONE, modifiedInsets.getInsets(WindowInsetsCompat.Type.ime()));
        verify(mUpdateRunnable, never()).run();
    }

    @Test
    public void testUnanimatedChange_appliedImmediately() {
        WindowInsetsCompat windowInsets =
                mBaseWindowInsets
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 384))
                        .build();
        WindowInsetsCompat modifiedInsets = mCallback.onApplyWindowInsets(mView, windowInsets);

        assertEquals(Insets.NONE, modifiedInsets.getInsets(WindowInsetsCompat.Type.ime()));
        verify(mUpdateRunnable).run();
        assertEquals(300, mCallback.getCurrentKeyboardHeight());
    }

    @Test
    public void testAttachDetach() {
        mCallback.attach(mWindowAndroid);
        verify(mInsetObserver).addWindowInsetsAnimationListener(mCallback);
        verify(mInsetObserver).addInsetsConsumer(mCallback);

        mCallback.detach();
        verify(mInsetObserver).removeWindowInsetsAnimationListener(mCallback);
        verify(mInsetObserver).removeInsetsConsumer(mCallback);
    }

    @Test
    public void testAttachDetach_ActivityFinishing() {
        when(mActivity.isFinishing()).thenReturn(true);
        mCallback.attach(mWindowAndroid);
        verify(mInsetObserver, never()).addWindowInsetsAnimationListener(mCallback);
        verify(mInsetObserver, never()).addInsetsConsumer(mCallback);

        mCallback.detach();
        verify(mInsetObserver, never()).removeWindowInsetsAnimationListener(mCallback);
        verify(mInsetObserver, never()).removeInsetsConsumer(mCallback);
    }

    @Test
    public void testConcurrentAnimations_respectFinalOnly() {
        mCallback.attach(mWindowAndroid);
        mCallback.onPrepare(mAnimation);

        WindowInsetsCompat windowInsets =
                mBaseWindowInsets
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, 384))
                        .build();
        WindowInsetsCompat modifiedInsets = mCallback.onApplyWindowInsets(mView, windowInsets);

        assertEquals(Insets.NONE, modifiedInsets.getInsets(WindowInsetsCompat.Type.ime()));
        verify(mUpdateRunnable, never()).run();

        mCallback.onPrepare(mAnimation2);
        windowInsets =
                mBaseWindowInsets.setInsets(WindowInsetsCompat.Type.ime(), Insets.NONE).build();
        mCallback.onApplyWindowInsets(mView, windowInsets);
        // The hide cancelled out the show, so there's no effective update.
        verify(mUpdateRunnable, never()).run();
        assertEquals(0, mCallback.getCurrentKeyboardHeight());

        mCallback.onEnd(mAnimation2);
        assertEquals(0, mCallback.getCurrentKeyboardHeight());

        // The height change associated with the initial animation should be ignored.
        verify(mUpdateRunnable, never()).run();
        mCallback.onEnd(mAnimation);
        assertEquals(0, mCallback.getCurrentKeyboardHeight());
    }
}
