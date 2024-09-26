// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.Window;

import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link SuggestionsListAnimationDriver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionsListAnimationDriverTest {

    private static final int VERTICAL_OFFSET = 20;
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private SuggestionsListAnimationDriver mDriver;
    private WindowInsetsAnimationCompat mImeAnimation;
    private WindowInsetsAnimationCompat mNonImeAnimation;
    private PropertyModel mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
    @Mock InsetObserver mInsetObserver;
    @Mock Runnable mShowRunnable;
    @Mock Window mWindow;
    @Mock View mRootView;

    private WindowInsetsControllerCompat mWindowInsetsControllerCompat;
    private float mTranslation;
    private Handler mHandler;

    @Before
    public void setUp() {
        mTranslation = 0.0f;
        doReturn(mRootView).when(mWindow).getDecorView();
        mImeAnimation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        mNonImeAnimation =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.statusBars(), null, 160);
        mWindowInsetsControllerCompat = new WindowInsetsControllerCompat(mWindow, mRootView);
        mHandler = new Handler(Looper.getMainLooper());
        mDriver =
                new SuggestionsListAnimationDriver(
                        mInsetObserver,
                        mListModel,
                        () -> mTranslation,
                        mShowRunnable,
                        VERTICAL_OFFSET,
                        mHandler,
                        mWindow);
        mDriver.onViewAttachedToWindow(mRootView);
    }

    @Test
    public void testImeNotControllable() {
        mDriver.onControllableInsetsChanged(
                mWindowInsetsControllerCompat, WindowInsetsCompat.Type.statusBars());
        mDriver.onOmniboxSessionStateChange(true);
        verify(mInsetObserver, never()).addWindowInsetsAnimationListener(mDriver);
        verify(mShowRunnable, never()).run();

        mDriver.onPrepare(mImeAnimation);
        verify(mShowRunnable, never()).run();
    }

    @Test
    public void testTimeout() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(OmniboxMetrics.HISTOGRAM_FOCUS_TO_IME_ANIMATION_START)
                        .build();
        mDriver.onControllableInsetsChanged(
                mWindowInsetsControllerCompat, WindowInsetsCompat.Type.ime());
        mDriver.onOmniboxSessionStateChange(true);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mShowRunnable).run();
        // Driver should keep listening for onPrepare for metrics purposes.
        verify(mInsetObserver, never()).removeWindowInsetsAnimationListener(mDriver);

        mDriver.onPrepare(mImeAnimation);

        verify(mShowRunnable).run();
        verify(mInsetObserver).removeWindowInsetsAnimationListener(mDriver);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testTimeoutCancelledByOnPrepare() {
        mDriver.onControllableInsetsChanged(
                mWindowInsetsControllerCompat, WindowInsetsCompat.Type.ime());
        mDriver.onOmniboxSessionStateChange(true);

        mDriver.onPrepare(mImeAnimation);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mShowRunnable).run();
    }

    @Test
    public void testShowAnimation() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(OmniboxMetrics.HISTOGRAM_FOCUS_TO_IME_ANIMATION_START)
                        .build();
        mDriver.onControllableInsetsChanged(
                mWindowInsetsControllerCompat, WindowInsetsCompat.Type.ime());
        mDriver.onOmniboxSessionStateChange(true);
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.0f, MathUtils.EPSILON);
        verify(mInsetObserver).addWindowInsetsAnimationListener(mDriver);
        verify(mShowRunnable, never()).run();

        mDriver.onPrepare(mImeAnimation);
        verify(mShowRunnable).run();
        mImeAnimation.setFraction(0.4f);
        mTranslation = 200.0f;
        mDriver.onProgress(new WindowInsetsCompat.Builder().build(), List.of(mImeAnimation));

        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.4f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                200.0f + VERTICAL_OFFSET * 0.6f,
                MathUtils.EPSILON);

        mImeAnimation.setFraction(0.7f);
        mTranslation = 100.0f;
        mDriver.onProgress(new WindowInsetsCompat.Builder().build(), List.of(mImeAnimation));

        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.7f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                100.0f + VERTICAL_OFFSET * 0.3f,
                MathUtils.EPSILON);

        mTranslation = 0.0f;
        mImeAnimation.setFraction(0.95f);
        mDriver.onProgress(new WindowInsetsCompat.Builder().build(), List.of(mImeAnimation));
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                VERTICAL_OFFSET * 0.05f,
                MathUtils.EPSILON);

        // Reported omnibox translation no longer matters once the animation is complete.
        mTranslation = 10.0f;
        mDriver.onEnd(mImeAnimation);
        verify(mInsetObserver).removeWindowInsetsAnimationListener(mDriver);
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 1.0f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                0.0f,
                MathUtils.EPSILON);

        mImeAnimation.setFraction(0.9f);
        mDriver.onProgress(new WindowInsetsCompat.Builder().build(), List.of(mImeAnimation));
        /// Progress updates after onEnd should be ignored.
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 1.0f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                0.0f,
                MathUtils.EPSILON);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testNonImeAnimation() {
        mDriver.onControllableInsetsChanged(
                mWindowInsetsControllerCompat, WindowInsetsCompat.Type.ime());
        mDriver.onOmniboxSessionStateChange(true);
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.0f, MathUtils.EPSILON);

        mDriver.onPrepare(mNonImeAnimation);
        verify(mShowRunnable, never()).run();

        mNonImeAnimation.setFraction(0.2f);
        mDriver.onProgress(new WindowInsetsCompat.Builder().build(), List.of(mNonImeAnimation));
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.0f, MathUtils.EPSILON);

        mDriver.onPrepare(mImeAnimation);
        mNonImeAnimation.setFraction(0.3f);
        mImeAnimation.setFraction(0.1f);
        mDriver.onProgress(
                new WindowInsetsCompat.Builder().build(), List.of(mNonImeAnimation, mImeAnimation));
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.1f, MathUtils.EPSILON);

        // Ending the non-ime animation should have no effect
        mDriver.onEnd(mNonImeAnimation);
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.1f, MathUtils.EPSILON);
        verify(mInsetObserver, never()).removeWindowInsetsAnimationListener(mDriver);
    }
}
