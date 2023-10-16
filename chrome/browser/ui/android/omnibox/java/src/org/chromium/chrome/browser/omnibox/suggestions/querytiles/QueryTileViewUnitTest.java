// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.querytiles;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.test.R;

/** Tests for {@link QueryTileView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class QueryTileViewUnitTest {
    private Context mContext;
    private QueryTileView mView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mView = new QueryTileView(mContext);
    }

    @Test
    public void setSelected_altersFocusedStateForKeyboardNavigation() {
        assertFalse(mView.isFocused());

        mView.setSelected(true);
        assertTrue(mView.isFocused());

        mView.setSelected(false);
        assertFalse(mView.isFocused());
    }
}
