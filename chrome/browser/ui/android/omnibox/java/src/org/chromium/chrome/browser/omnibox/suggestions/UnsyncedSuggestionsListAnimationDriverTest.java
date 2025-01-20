// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.animation.ValueAnimator;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link UnsyncedSuggestionsListAnimationDriver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UnsyncedSuggestionsListAnimationDriverTest {

    private static final int VERTICAL_OFFSET = 20;
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private UnsyncedSuggestionsListAnimationDriver mDriver;
    private PropertyModel mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
    @Mock Runnable mShowRunnable;
    @Mock private ValueAnimator mValueAnimator;
    private boolean mShouldAnimate = true;

    @Before
    public void setUp() {
        mDriver =
                new UnsyncedSuggestionsListAnimationDriver(
                        mListModel, mShowRunnable, () -> mShouldAnimate, VERTICAL_OFFSET);
    }

    @Test
    public void testAnimationDisabled() {
        mShouldAnimate = false;
        assertFalse(mDriver.isAnimationEnabled());

        mShouldAnimate = true;
        assertTrue(mDriver.isAnimationEnabled());
    }

    @Test
    public void testRunAnimation() {
        mDriver.onOmniboxSessionStateChange(true);

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
    public void testCancel() {
        mDriver.onOmniboxSessionStateChange(true);
        mDriver.onOmniboxSessionStateChange(false);

        verify(mShowRunnable, times(2)).run();
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 1.0f, MathUtils.EPSILON);
        assertEquals(
                mListModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y),
                0.f,
                MathUtils.EPSILON);
    }
}
