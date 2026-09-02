// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tail;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

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
        mTailSuggestionView = spy(new TailSuggestionView(mContext));
        mBaseView = new BaseSuggestionView<>(mTailSuggestionView);
        mModel = new PropertyModel(TailSuggestionViewProperties.ALL_KEYS);
        mResourceProvider = new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);
        mModel.set(SuggestionCommonProperties.RESOURCE_PROVIDER, mResourceProvider);
        PropertyModelChangeProcessor.create(mModel, mBaseView, new TailSuggestionViewBinder());
    }

    @Test
    public void tailSuggestionView_setAlignmentManager() {
        AlignmentManager alignmentManager = new AlignmentManager();

        mModel.set(TailSuggestionViewProperties.ALIGNMENT_MANAGER, alignmentManager);
        verify(mTailSuggestionView).setAlignmentManager(alignmentManager);
    }

    @Test
    public void tailSuggestionView_setTailText() {
        final SuggestionSpannable span = new SuggestionSpannable("test");

        mModel.set(TailSuggestionViewProperties.TEXT, span);
        verify(mTailSuggestionView).setTailText(span);
    }

    @Test
    public void tailSuggestionView_setFullText() {
        final String test = "test";

        mModel.set(TailSuggestionViewProperties.FILL_INTO_EDIT, test);
        verify(mTailSuggestionView).setFullText(test);
    }

    @Test
    public void tailSuggestionView_setTextColor() {
        final @BrandedColorScheme int colorScheme = BrandedColorScheme.LIGHT_BRANDED_THEME;
        mResourceProvider.setBrandedColorScheme(colorScheme);
        final @ColorInt int color = mResourceProvider.getSuggestionPrimaryTextColor();

        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, colorScheme);
        verify(mTailSuggestionView).setTextColor(color);
    }
}
