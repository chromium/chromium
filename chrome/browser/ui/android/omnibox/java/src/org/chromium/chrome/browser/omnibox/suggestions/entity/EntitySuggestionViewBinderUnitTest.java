// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link EntitySuggestionViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class EntitySuggestionViewBinderUnitTest {
    private BaseSuggestionView<View> mView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mView =
                new BaseSuggestionView<View>(
                        ContextUtils.getApplicationContext(), R.layout.omnibox_basic_suggestion);
        mModel = new PropertyModel(EntitySuggestionViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, EntitySuggestionViewBinder::bind);
    }

    @Test
    public void bind_passesCallsToSuggestionViewBinder() {
        mModel.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, new SuggestionSpannable("abc"));
        Assert.assertEquals("abc", mView.<TextView>findViewById(R.id.line_1).getText().toString());
    }
}
