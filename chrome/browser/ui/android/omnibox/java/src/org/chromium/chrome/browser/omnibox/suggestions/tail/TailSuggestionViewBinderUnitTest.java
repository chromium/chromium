// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tail;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link TailSuggestionViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TailSuggestionViewBinderUnitTest {
    private PropertyModel mModel;
    private Context mContext;

    private TailSuggestionView mTailSuggestionView;
    private BaseSuggestionView<TailSuggestionView> mBaseView;
    private OmniboxResourceProvider mResourceProvider;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mTailSuggestionView = new TailSuggestionView(mContext);
        mResourceProvider = new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);
        mModel =
                new PropertyModel.Builder(TailSuggestionViewProperties.ALL_KEYS)
                        .with(SuggestionCommonProperties.RESOURCE_PROVIDER, mResourceProvider)
                        .build();
        mBaseView = new BaseSuggestionView<>(mTailSuggestionView);
        PropertyModelChangeProcessor.create(mModel, mBaseView, new TailSuggestionViewBinder());
    }

    @Test
    public void tailSuggestionView_setAlignmentManager() {
        AlignmentManager alignmentManager = new AlignmentManager();

        mModel.set(TailSuggestionViewProperties.ALIGNMENT_MANAGER, alignmentManager);
        assertEquals(alignmentManager, mTailSuggestionView.getAlignmentManagerForTesting());
    }

    @Test
    public void tailSuggestionView_setTailText() {
        final SuggestionSpannable span = new SuggestionSpannable("test");

        mModel.set(TailSuggestionViewProperties.TEXT, span);
        assertEquals(span.toString(), mTailSuggestionView.getText().toString());
    }

    @Test
    public void tailSuggestionView_setFullText() {
        final String test = "test";

        mModel.set(TailSuggestionViewProperties.FILL_INTO_EDIT, test);
        assertEquals(
                (int) mTailSuggestionView.getPaint().measureText(test, 0, test.length()),
                mTailSuggestionView.getFullTextWidthForTesting());
    }

    @Test
    public void tailSuggestionView_setTextColor() {
        final @BrandedColorScheme int colorScheme = BrandedColorScheme.LIGHT_BRANDED_THEME;
        mResourceProvider.setBrandedColorScheme(colorScheme);
        final @ColorInt int color = mResourceProvider.getSuggestionPrimaryTextColor();

        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, colorScheme);
        assertEquals(color, mTailSuggestionView.getCurrentTextColor());
    }
}
