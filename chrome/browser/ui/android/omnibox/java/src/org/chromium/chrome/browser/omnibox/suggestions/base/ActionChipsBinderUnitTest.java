// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties.FormFactor;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link ActionChipsBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionChipsBinderUnitTest {
    private ActionChipsView mView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mView = new ActionChipsView(context);
        mModel =
                new PropertyModel.Builder(BaseSuggestionViewProperties.ALL_KEYS)
                        .with(
                                SuggestionCommonProperties.COLOR_SCHEME,
                                BrandedColorScheme.APP_DEFAULT)
                        .build();
        PropertyModelChangeProcessor.create(mModel, mView, ActionChipsBinder::bind);
    }

    private ListItem buildChipItem(String text) {
        PropertyModel chipModel =
                new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                        .with(ChipProperties.TEXT, text)
                        .with(ChipProperties.CONTENT_DESCRIPTION, text)
                        .with(ChipProperties.ENABLED, true)
                        .with(
                                ChipProperties.PRIMARY_TEXT_APPEARANCE,
                                R.style.TextAppearance_ChipText)
                        .build();
        return new ListItem(ActionChipsProperties.ViewType.CHIP, chipModel);
    }

    @Test
    public void bindActionChips_nonNullList_setsAdapterAndVisible() {
        ModelList list = new ModelList();
        list.add(buildChipItem("Call"));
        list.add(buildChipItem("Directions"));

        mModel.set(ActionChipsProperties.ACTION_CHIPS, list);

        assertEquals(View.VISIBLE, mView.getVisibility());
        assertNotNull(mView.getAdapter());
        assertEquals(2, mView.getAdapter().getItemCount());
    }

    @Test
    public void bindActionChips_nullList_clearsAdapterAndHidesView() {
        ModelList list = new ModelList();
        list.add(buildChipItem("Call"));
        mModel.set(ActionChipsProperties.ACTION_CHIPS, list);
        assertEquals(View.VISIBLE, mView.getVisibility());

        mModel.set(ActionChipsProperties.ACTION_CHIPS, null);

        assertEquals(View.GONE, mView.getVisibility());
        assertNull(mView.getAdapter());
    }

    @Test
    public void bindDeviceFormFactor_setsHorizontalFadingEdge() {
        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.TABLET);
        assertTrue(mView.isHorizontalFadingEdgeEnabled());

        mModel.set(SuggestionCommonProperties.DEVICE_FORM_FACTOR, FormFactor.PHONE);
        assertFalse(mView.isHorizontalFadingEdgeEnabled());
    }
}
