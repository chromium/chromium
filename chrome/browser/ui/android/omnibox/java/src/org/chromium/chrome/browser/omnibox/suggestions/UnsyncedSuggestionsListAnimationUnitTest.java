// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.animation.ValueAnimator;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link UnsyncedSuggestionsListAnimation}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UnsyncedSuggestionsListAnimationUnitTest {

    private static final int VERTICAL_OFFSET = 20;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private UnsyncedSuggestionsListAnimation mDriver;
    private final PropertyModel mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
    private final boolean mIsToolbarBottomAnchored = true;
    @Mock private Runnable mShowRunnable;
    @Mock private ValueAnimator mValueAnimator;
    private Context mContext;
    private float mTranslation;
    private boolean mIsPopover;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mDriver =
                new UnsyncedSuggestionsListAnimation(
                        mListModel,
                        mShowRunnable,
                        () -> mIsToolbarBottomAnchored,
                        () -> mTranslation,
                        () -> mIsPopover,
                        mContext);
    }

    @Test
    public void testRunAnimation() {
        mDriver.getAnimator().start();

        verify(mShowRunnable).run();
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.0f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                VERTICAL_OFFSET,
                MathUtils.EPSILON);

        doReturn(0.5f).when(mValueAnimator).getAnimatedFraction();
        mDriver.onAnimationUpdate(mValueAnimator);
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.5f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                VERTICAL_OFFSET / 2,
                MathUtils.EPSILON);

        mDriver.onAnimationEnd(mValueAnimator);
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 1.0f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                0.f,
                MathUtils.EPSILON);
    }

    @Test
    public void testRunAnimationWithNtpTranslation() {
        mTranslation = 200.0f;

        mDriver.getAnimator().start();

        verify(mShowRunnable).run();
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.0f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                200.0f + VERTICAL_OFFSET,
                MathUtils.EPSILON);

        doReturn(0.5f).when(mValueAnimator).getAnimatedFraction();
        mTranslation = 100.0f;
        mDriver.onAnimationUpdate(mValueAnimator);

        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.5f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                100.0f + VERTICAL_OFFSET * 0.5f,
                MathUtils.EPSILON);

        doReturn(0.7f).when(mValueAnimator).getAnimatedFraction();
        mTranslation = 23.0f;
        mDriver.onAnimationUpdate(mValueAnimator);

        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.7f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                23.0f + VERTICAL_OFFSET * 0.3f,
                MathUtils.EPSILON);

        doReturn(0.95f).when(mValueAnimator).getAnimatedFraction();
        mTranslation = 0.f;
        mDriver.onAnimationUpdate(mValueAnimator);

        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.95f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                VERTICAL_OFFSET * 0.05f,
                MathUtils.EPSILON);
    }

    @Test
    public void testCancel() {
        mDriver.getAnimator().start();
        mDriver.onOmniboxSessionStateChange(false);

        verify(mShowRunnable, times(2)).run();
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 1.0f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                0.f,
                MathUtils.EPSILON);
    }

    @Test
    public void testNoopAnimationOnDesktop() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        try {
            mDriver.getAnimator().start();
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

            verify(mShowRunnable).run();
            // Since duration is 0, it should have finished immediately.
            assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 1.0f, MathUtils.EPSILON);
            assertEquals(
                    mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                    0.f,
                    MathUtils.EPSILON);
        } finally {
            OmniboxCapabilities.setIsDesktopPlatformForTesting(null);
        }
    }

    @Test
    public void testPopoverAnimation() {
        mIsPopover = true;
        mDriver.getAnimator().start();

        verify(mShowRunnable).run();
        // Should start at 0 alpha
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.0f, MathUtils.EPSILON);
        // Translation should NOT be set (default is 0.0f)
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                0.f,
                MathUtils.EPSILON);

        doReturn(0.5f).when(mValueAnimator).getAnimatedFraction();
        mDriver.onAnimationUpdate(mValueAnimator);
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.5f, MathUtils.EPSILON);
        // Still no translation
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                0.f,
                MathUtils.EPSILON);

        mDriver.onAnimationEnd(mValueAnimator);
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 1.0f, MathUtils.EPSILON);
    }
}
