// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.spy;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link AnswerSuggestionViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AnswerSuggestionViewBinderTest {
    private Context mContext;
    private PropertyModel mModel;
    private BaseSuggestionView<View> mBaseView;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();

        mBaseView = spy(new BaseSuggestionView(new LinearLayout(mContext)));

        mModel = new PropertyModel(AnswerSuggestionViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mBaseView, AnswerSuggestionViewBinder::bind);
    }

    @Test
    public void setPadding() {
        mModel.set(AnswerSuggestionViewProperties.RIGHT_PADDING, 13);
        assertEquals(13, mBaseView.getPaddingRight());
    }
}
