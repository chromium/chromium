// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.ContextThemeWrapper;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link HeaderViewBinder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class HeaderViewBinderUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    Activity mActivity;
    PropertyModel mModel;
    Context mContext;
    Resources mResources;

    HeaderView mHeaderView;

    @Before
    public void setUp() {
        mContext = new ContextThemeWrapper(
                ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mResources = mContext.getResources();

        MockitoAnnotations.initMocks(this);
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mHeaderView = mock(HeaderView.class,
                Mockito.withSettings().useConstructor(mActivity).defaultAnswer(
                        Mockito.CALLS_REAL_METHODS));

        mModel = new PropertyModel(HeaderViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mHeaderView, HeaderViewBinder::bind);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    public void headerView_useModernizedHeaderPaddingFalse() {
        mModel.set(HeaderViewProperties.USE_MODERNIZED_HEADER_PADDING, false);

        int minHeight = mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height);
        int paddingStart =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_start);
        int paddingTop =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_top);
        int paddingBottom =
                mResources.getDimensionPixelSize(R.dimen.omnibox_suggestion_header_padding_bottom);
        verify(mHeaderView, times(1))
                .setUpdateHeaderPadding(minHeight, paddingStart, paddingTop, paddingBottom);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE)
    public void headerView_useModernizedHeaderPadding_smallestMargins() {
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALLEST_MARGINS.setForTesting(true);
        mModel.set(HeaderViewProperties.USE_MODERNIZED_HEADER_PADDING, true);

        int minHeight = mResources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_header_height_modern_phase2_smallest);
        int paddingStart = mResources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_header_padding_start_modern_smallest);
        int paddingTop = mResources.getDimensionPixelSize(
                R.dimen.omnibox_suggestion_header_padding_top_smallest);
        int paddingBottom = 0;
        verify(mHeaderView, times(1))
                .setUpdateHeaderPadding(minHeight, paddingStart, paddingTop, paddingBottom);
    }
}
