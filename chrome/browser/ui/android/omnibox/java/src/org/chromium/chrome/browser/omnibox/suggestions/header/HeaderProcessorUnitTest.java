// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link HeaderProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HeaderProcessorUnitTest {
    private Context mContext;
    private PropertyModel mModel;
    private HeaderProcessor mProcessor;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mProcessor = new HeaderProcessor(mContext);
        mModel = mProcessor.createModel();
    }

    @Test
    public void getViewTypeId() {
        assertEquals(OmniboxSuggestionUiType.HEADER, mProcessor.getViewTypeId());
    }

    @Test
    public void getMinimumHeight() {
        assertEquals(
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height),
                mProcessor.getMinimumViewHeight());
    }

    @Test
    public void populateModel() {
        assertNull(mModel.get(HeaderViewProperties.TITLE));
        mProcessor.populateModel(mModel, "title");
        assertEquals("title", mModel.get(HeaderViewProperties.TITLE));
    }
}
