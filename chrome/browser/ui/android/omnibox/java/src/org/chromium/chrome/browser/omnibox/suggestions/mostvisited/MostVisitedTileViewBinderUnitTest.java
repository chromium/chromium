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
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link MostVisitedTileViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MostVisitedTileViewBinderUnitTest {
    private PropertyModel mModel;
    private Context mContext;
    private TileView mView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mView = spy(new TileView(mContext, null));
        mModel = new PropertyModel(MostVisitedTileViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mView, MostVisitedTileViewBinder::bind);
    }

    @Test
    public void setColorScheme() {
        // Very rudimentary test confirming that tile background is updated when color scheme
        // changes.
        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT);
        verify(mView).setBackground(any());

        clearInvocations(mView);

        mModel.set(SuggestionCommonProperties.COLOR_SCHEME, BrandedColorScheme.INCOGNITO);
        verify(mView).setBackground(any());
    }
}
