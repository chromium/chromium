// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.dividerline;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link DividerLineViewBinder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class DividerLineViewBinderUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    Activity mActivity;
    PropertyModel mModel;

    DividerLineView mDividerLineView;
    @Mock
    View mDividerLine;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        mDividerLineView = mock(DividerLineView.class,
                Mockito.withSettings().useConstructor(mActivity).defaultAnswer(
                        Mockito.CALLS_REAL_METHODS));

        when(mDividerLineView.getDivider()).thenReturn(mDividerLine);

        mModel = new PropertyModel(SuggestionCommonProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mDividerLineView, DividerLineViewBinder::bind);
    }

    @Test
    @SmallTest
    public void colorBindTest() {
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.LIGHT_BRANDED_THEME);
        // We do not verify the color value exactly since the color is dynamic, since we cannot
        // guarantee that the test device is using the default theme color.
        verify(mDividerLine, times(1)).setBackgroundColor(anyInt());
        verify(mDividerLine, never()).setBackgroundResource(anyInt());
    }

    @Test
    @SmallTest
    public void colorBindTest_Incognito() {
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.INCOGNITO);
        verify(mDividerLine, times(1)).setBackgroundResource(R.color.divider_line_bg_color_light);
        verify(mDividerLine, never()).setBackgroundColor(anyInt());
    }
}
