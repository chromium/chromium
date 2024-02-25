// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link HeaderViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HeaderViewBinderUnitTest {
    private PropertyModel mModel;
    private HeaderView mView;

    @Before
    public void setUp() {
        mView = spy(new HeaderView(ContextUtils.getApplicationContext()));
        mModel = new PropertyModel(HeaderViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, HeaderViewBinder::bind);
    }

    @Test
    public void setTitle() {
        mModel.set(HeaderViewProperties.TITLE, "text");
        verify(mView).setText("text");
        mModel.set(HeaderViewProperties.TITLE, null);
        verify(mView).setText(null);
        mModel.set(HeaderViewProperties.TITLE, "text2");
        verify(mView).setText("text2");
    }

    @Test
    public void setLayoutDirection() {
        assertEquals(View.LAYOUT_DIRECTION_LTR, mView.getLayoutDirection());

        mModel.set(SuggestionCommonProperties.LAYOUT_DIRECTION, View.LAYOUT_DIRECTION_RTL);
        verify(mView).setLayoutDirection(View.LAYOUT_DIRECTION_RTL);

        mModel.set(SuggestionCommonProperties.LAYOUT_DIRECTION, View.LAYOUT_DIRECTION_LTR);
        verify(mView).setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
    }

    @Test
    public void setColorScheme() {
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);
        verify(mView).setTextAppearance(R.style.TextAppearance_TextMediumThick_Secondary);

        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.INCOGNITO);
        verify(mView)
                .setTextAppearance(R.style.TextAppearance_TextMediumThick_Secondary_Baseline_Light);
    }
}
