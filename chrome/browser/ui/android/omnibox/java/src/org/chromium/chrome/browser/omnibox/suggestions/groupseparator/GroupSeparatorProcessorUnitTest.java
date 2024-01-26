// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.groupseparator;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;

/** Tests for {@link GroupSeparatorProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class GroupSeparatorProcessorUnitTest {
    private GroupSeparatorProcessor mProcessor;

    @Before
    public void setUp() {
        mProcessor = new GroupSeparatorProcessor(ContextUtils.getApplicationContext());
    }

    @Test
    @SmallTest
    public void basicInfoTest() {
        Assert.assertEquals(OmniboxSuggestionUiType.GROUP_SEPARATOR, mProcessor.getViewTypeId());

        int minimumHeight =
                ContextUtils.getApplicationContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.divider_height)
                        + ContextUtils.getApplicationContext()
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen.omnibox_suggestion_list_divider_line_padding);
        Assert.assertEquals(minimumHeight, mProcessor.getMinimumViewHeight());

        Assert.assertNotNull(mProcessor.createModel());
    }
}
