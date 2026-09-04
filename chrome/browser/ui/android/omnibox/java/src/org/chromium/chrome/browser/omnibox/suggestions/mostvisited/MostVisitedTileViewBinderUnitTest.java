// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link MostVisitedTileViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MostVisitedTileViewBinderUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private PropertyModel mModel;
    private Context mContext;
    private TileView mView;
    private OmniboxResourceProvider mResourceProvider;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mView = spy(new TileView(mContext, null));
        mModel = new PropertyModel(MostVisitedTileViewProperties.ALL_KEYS);
        mResourceProvider = new OmniboxResourceProvider(mContext, BrandedColorScheme.APP_DEFAULT);
        PropertyModelChangeProcessor.create(
                mModel, mView, new MostVisitedTileViewBinder(mResourceProvider));
    }

    @Test
    public void setColorScheme() {
        // Very rudimentary test confirming that tile background is updated when color scheme
        // changes.
        mResourceProvider.setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);
        verify(mView).setBackground(any());

        clearInvocations(mView);

        mResourceProvider.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.INCOGNITO);
        verify(mView).setBackground(any());
    }
}
