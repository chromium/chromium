// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.Context;

import androidx.annotation.NonNull;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link BaseCarouselSuggestionProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseCarouselSuggestionProcessorUnitTest {
    private static final int ITEM_VIEW_WIDTH = 12345;
    public @Rule TestRule mFeatures = new Features.JUnitProcessor();

    // Stores PropertyModel for the suggestion.
    private PropertyModel mModel;
    private BaseCarouselSuggestionProcessorTestClass mProcessor;

    /** Test class to instantiate BaseCarouselSuggestionProcessor class */
    public class BaseCarouselSuggestionProcessorTestClass extends BaseCarouselSuggestionProcessor {
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
            return 0;
        }

        @Override
        public int getCarouselItemViewWidth() {
            return ITEM_VIEW_WIDTH;
        }
    }

    @Before
    public void setUp() {
        mProcessor =
                new BaseCarouselSuggestionProcessorTestClass(ContextUtils.getApplicationContext());
        mModel = mProcessor.createModel();
    }

    @Test
    public void testPopulateModelTest_notTablet() {
        mProcessor.onNativeInitialized();
        mProcessor.populateModel(null, mModel, 0);
        Assert.assertFalse(mModel.get(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE));
    }

    @Test
    @Config(qualifiers = "w600dp-h820dp")
    public void testPopulateModelTest_isTablet() {
        mProcessor.onNativeInitialized();
        mProcessor.populateModel(null, mModel, 0);
        Assert.assertTrue(mModel.get(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE));
    }

    @Test
    public void testPopulateItemViewWidth() {
        mProcessor.onNativeInitialized();
        mProcessor.populateModel(null, mModel, 0);
        Assert.assertEquals(
                ITEM_VIEW_WIDTH, mModel.get(BaseCarouselSuggestionViewProperties.ITEM_WIDTH));
    }
}
