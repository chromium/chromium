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
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link BaseCarouselSuggestionProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseCarouselSuggestionProcessorUnitTest {
    private static final int ITEM_VIEW_WIDTH = 12345;
    public @Rule TestRule mFeatures = new Features.JUnitProcessor();

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
    public void testPopulateModelTest_notTablet() {
        mProcessor.onNativeInitialized();
        mProcessor.populateModel(null, mModel, 0);
        Assert.assertFalse(mModel.get(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE));
    }

    @Test
    @Config(qualifiers = "w600dp-h820dp")
    @DisableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    public void testPopulateModelTest_isTabletWithoutRevamp() {
        mProcessor.onNativeInitialized();
        mProcessor.populateModel(null, mModel, 0);
        Assert.assertTrue(mModel.get(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE));
    }

    @Test
    @Config(qualifiers = "w600dp-h820dp")
    @EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    public void testPopulateModelTest_isTabletWithRevamp() {
        // Revamp turns off horizontal fading edge.
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        mProcessor.onNativeInitialized();
        mProcessor.populateModel(null, mModel, 0);
        Assert.assertFalse(mModel.get(BaseCarouselSuggestionViewProperties.HORIZONTAL_FADE));
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

    @Test
    public void allowBackgroundRounding_disallowedAsCarouselHandlesThisInternally() {
        Assert.assertFalse(mProcessor.allowBackgroundRounding());
    }
}
