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
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link UnsyncedSuggestionsListAnimationDriver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UnsyncedSuggestionsListAnimationDriverTest {

    private static final int VERTICAL_OFFSET = 20;
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private UnsyncedSuggestionsListAnimationDriver mDriver;
    private final PropertyModel mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
    @Mock Runnable mShowRunnable;
    @Mock private ValueAnimator mValueAnimator;
    private Context mContext;
    private boolean mIsToolbarBottomAnchored = true;
    private float mTranslation;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mDriver =
                new UnsyncedSuggestionsListAnimationDriver(
                        mListModel,
                        mShowRunnable,
                        () -> mIsToolbarBottomAnchored,
                        () -> mTranslation,
                        mContext);
    }

    @Test
    @DisableFeatures({OmniboxFeatureList.ANIMATE_SUGGESTIONS_LIST_APPEARANCE})
    public void testAnimationDisabled() {
        mIsToolbarBottomAnchored = false;
        assertFalse(mDriver.isAnimationEnabled());

        mIsToolbarBottomAnchored = true;
        assertTrue(mDriver.isAnimationEnabled());
    }

    @Test
    @EnableFeatures({OmniboxFeatureList.ANIMATE_SUGGESTIONS_LIST_APPEARANCE})
    public void testAnimationFlagEnabled() {
        mIsToolbarBottomAnchored = false;
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
    public void testRunAnimationWithNtpTranslation() {
        mTranslation = 200.0f;

        mDriver.onOmniboxSessionStateChange(true);

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
