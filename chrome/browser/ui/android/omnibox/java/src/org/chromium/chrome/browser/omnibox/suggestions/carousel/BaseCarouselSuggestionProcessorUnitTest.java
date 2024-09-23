// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.Context;

import androidx.annotation.NonNull;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link BaseCarouselSuggestionProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseCarouselSuggestionProcessorUnitTest {

    private Context mContext;
    private PropertyModel mModel;
    private BaseCarouselSuggestionProcessorTestClass mProcessor;

    /** Test class to instantiate BaseCarouselSuggestionProcessor class */
    public static class BaseCarouselSuggestionProcessorTestClass
            extends BaseCarouselSuggestionProcessor {
        public static int sReportedItemViewHeight;

        /**
         * Constructs a new BaseCarouselSuggestionProcessor.
         *
         * @param context Current context.
         */
        public BaseCarouselSuggestionProcessorTestClass(@NonNull Context context) {
            super(context);
        }

        @Override
        public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int matchIndex) {
            return false;
        }

        @Override
        public int getViewTypeId() {
            return 0;
        }

        @Override
        public PropertyModel createModel() {
            return new PropertyModel(BaseCarouselSuggestionViewProperties.ALL_KEYS);
        }

        @Override
        public int getCarouselItemViewHeight() {
            return sReportedItemViewHeight;
        }
    }

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mProcessor = new BaseCarouselSuggestionProcessorTestClass(mContext);
        mModel = mProcessor.createModel();
    }

    @Test
    public void getMinimumViewHeight_includesDecorations() {
        int baseHeight =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height);

        BaseCarouselSuggestionProcessorTestClass.sReportedItemViewHeight = 0;
        Assert.assertEquals(baseHeight, mProcessor.getMinimumViewHeight());

        BaseCarouselSuggestionProcessorTestClass.sReportedItemViewHeight = 100;
        Assert.assertEquals(100 + baseHeight, mProcessor.getMinimumViewHeight());
    }
}
