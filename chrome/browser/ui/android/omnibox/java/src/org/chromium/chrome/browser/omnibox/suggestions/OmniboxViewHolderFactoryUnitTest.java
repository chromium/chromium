// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter.ViewHolder;

import java.util.Set;

/** Unit tests for the OmniboxViewHolderFactory component. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxViewHolderFactoryUnitTest {
    public static final Set<Integer> OBSOLETE_UI_TYPES =
            Set.of(OmniboxSuggestionUiType.OBSOLETE_QUERY_TILES);

    private FrameLayout mContainer;
    private OmniboxViewHolderFactory mFactory;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mContainer = new FrameLayout(context);
        mFactory = new OmniboxViewHolderFactory();
    }

    @Test
    public void createViewHolder_retainsItemsByType() {
        for (@OmniboxSuggestionUiType int type = OmniboxSuggestionUiType.DEFAULT;
                type < OmniboxSuggestionUiType.COUNT;
                type++) {
            if (OBSOLETE_UI_TYPES.contains(type)) continue;

            ViewHolder viewHolder = mFactory.createViewHolderForPool(mContainer, type);
            // Each view type should have a corresponding view object.
            assertNotNull(viewHolder);
            assertNotNull(viewHolder.itemView);
            assertEquals(type, viewHolder.getItemViewType());
        }
    }
}
