// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetObserverSupplier;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link SuggestionsListAnimationDriver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionsListAnimationDriverTest {

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private SuggestionsListAnimationDriver mDriver;
    private WindowInsetsAnimationCompat mImeAnimation;
    private WindowInsetsAnimationCompat mNonImeAnimation;
    private PropertyModel mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
    @Mock private WindowAndroid mWindowAndroid;
    @Mock InsetObserver mInsetObserver;

    @Before
    public void setUp() {
        InsetObserverSupplier.setInstanceForTesting(mInsetObserver);
        mImeAnimation = new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.ime(), null, 160);
        mNonImeAnimation =
                new WindowInsetsAnimationCompat(WindowInsetsCompat.Type.statusBars(), null, 160);
        mDriver = new SuggestionsListAnimationDriver(mWindowAndroid, mListModel);
    }

    @Test
    public void testShowAnimation() {
        mDriver.onShowAnimationAboutToStart();
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.0f, MathUtils.EPSILON);
        verify(mInsetObserver).addWindowInsetsAnimationListener(mDriver);

        mDriver.onPrepare(mImeAnimation);
        mImeAnimation.setFraction(0.4f);
        mDriver.onProgress(new WindowInsetsCompat.Builder().build(), List.of(mImeAnimation));

        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.4f, MathUtils.EPSILON);

        mImeAnimation.setFraction(0.7f);
        mDriver.onProgress(new WindowInsetsCompat.Builder().build(), List.of(mImeAnimation));

        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.7f, MathUtils.EPSILON);

        mDriver.onEnd(mImeAnimation);
        verify(mInsetObserver).removeWindowInsetsAnimationListener(mDriver);
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 1.0f, MathUtils.EPSILON);

        mImeAnimation.setFraction(0.9f);
        mDriver.onProgress(new WindowInsetsCompat.Builder().build(), List.of(mImeAnimation));
        /// Progress updates after onEnd should be ignored.
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 1.0f, MathUtils.EPSILON);
    }

    @Test
    public void testNonImeAnimation() {
        mDriver.onShowAnimationAboutToStart();
        assertEquals(mListModel.get(SuggestionListProperties.ALPHA), 0.0f, MathUtils.EPSILON);

        mDriver.onPrepare(mNonImeAnimation);
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
