// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.components.browser_ui.widget.ListItemBuilder.buildSimpleMenuItem;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ListItemBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ListItemBuilderUnitTest {

    private static final @StringRes int FAKE_TITLE_ID = R.string.search_menu_title;
    private static final @IdRes int FAKE_MENU_ID = R.id.menu;
    private static final @DrawableRes int FAKE_START_ICON_ID = R.drawable.ic_call_answer;
    private static final @DrawableRes int FAKE_END_ICON_ID = R.drawable.ic_call_decline;
    private static final @ColorRes int FAKE_TINT_ID = R.color.background_material_light;
    private static final @StyleRes int FAKE_STYLE_ID = R.style.Base_AlertDialog_AppCompat;
    private static final String FAKE_TITLE_STRING = "TITLE";
    private static final String FAKE_CONTENT_DESC = "DESCRIPTION";

    @Test
    public void testBuild_defaultValues() {
        ListItem listItem = new ListItemBuilder().build();
        PropertyModel model = listItem.model;

        assertEquals(ListItemType.MENU_ITEM, listItem.type);
        assertTrue(model.get(ListMenuItemProperties.ENABLED));
        assertEquals(
                BrowserUiListMenuUtils.getDefaultTextAppearanceStyle(),
                model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        assertEquals(
                BrowserUiListMenuUtils.getDefaultIconTintColorStateListId(),
                model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID));

        assertNull(model.get(ListMenuItemProperties.TITLE));
        assertEquals(0, model.get(ListMenuItemProperties.TITLE_ID));
        assertFalse(model.get(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END));
        assertNull(model.get(ListMenuItemProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testBuild_withAllProperties_nonIncognito() {
        ListItemBuilder builder =
                new ListItemBuilder()
                        .withTitle(FAKE_TITLE_STRING)
                        .withTitleRes(FAKE_TITLE_ID)
                        .withMenuId(FAKE_MENU_ID)
                        .withStartIconRes(FAKE_START_ICON_ID)
                        .withEndIconRes(FAKE_END_ICON_ID)
                        .withEnabled(false)
                        .withContentDescription(FAKE_CONTENT_DESC)
                        .withIsTextEllipsizedAtEnd(true)
                        .withIsIncognito(false)
                        .withIconTintColorStateList(FAKE_TINT_ID)
                        .withTextAppearanceStyle(FAKE_STYLE_ID);

        ListItem listItem = builder.build();
        PropertyModel model = listItem.model;

        assertEquals(FAKE_TITLE_STRING, model.get(ListMenuItemProperties.TITLE));
        assertEquals(
                "Title ID should not be set when String title exists",
                0,
                model.get(ListMenuItemProperties.TITLE_ID));
        assertEquals(FAKE_MENU_ID, model.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(FAKE_START_ICON_ID, model.get(ListMenuItemProperties.START_ICON_ID));
        assertEquals(FAKE_END_ICON_ID, model.get(ListMenuItemProperties.END_ICON_ID));
        assertFalse(model.get(ListMenuItemProperties.ENABLED));
        assertEquals(FAKE_CONTENT_DESC, model.get(ListMenuItemProperties.CONTENT_DESCRIPTION));
        assertTrue(model.get(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END));

        // When not incognito, custom appearance/tint should not override defaults.
        assertEquals(
                BrowserUiListMenuUtils.getDefaultTextAppearanceStyle(),
                model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        assertEquals(
                BrowserUiListMenuUtils.getDefaultIconTintColorStateListId(),
                model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID));
    }

    @Test
    public void testBuild_titleIdOnly() {
        ListItem listItem = new ListItemBuilder().withTitleRes(FAKE_TITLE_ID).build();
        PropertyModel model = listItem.model;

        assertEquals(FAKE_TITLE_ID, model.get(ListMenuItemProperties.TITLE_ID));
        assertNull(model.get(ListMenuItemProperties.TITLE));
    }

    @Test
    public void testBuildSimpleMenuItem() {
        ListItem listItem = buildSimpleMenuItem(FAKE_TITLE_ID);
        PropertyModel model = listItem.model;

        assertEquals(FAKE_TITLE_ID, model.get(ListMenuItemProperties.TITLE_ID));
        assertNull(model.get(ListMenuItemProperties.TITLE));
    }

    @Test
    public void testBuild_incognito_withDefaults() {
        ListItem listItem = new ListItemBuilder().withIsIncognito(true).build();
        PropertyModel model = listItem.model;

        assertEquals(
                R.style.TextAppearance_DensityAdaptive_TextLarge_Primary_Baseline_Light,
                model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        assertEquals(
                R.color.default_icon_color_light_tint_list,
                model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID));
    }

    @Test
    public void testBuild_incognito_withCustomStyles() {
        ListItem listItem =
                new ListItemBuilder()
                        .withIsIncognito(true)
                        .withTextAppearanceStyle(FAKE_STYLE_ID)
                        .withIconTintColorStateList(FAKE_TINT_ID)
                        .build();
        PropertyModel model = listItem.model;

        assertEquals(FAKE_STYLE_ID, model.get(ListMenuItemProperties.TEXT_APPEARANCE_ID));
        assertEquals(FAKE_TINT_ID, model.get(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID));
    }

    @Test
    public void testBuild_noContentDescription() {
        ListItem listItem = new ListItemBuilder().build();
        PropertyModel model = listItem.model;

        assertNull(
                "Content description should not be present in the model if not set",
                model.get(ListMenuItemProperties.CONTENT_DESCRIPTION));
    }

    @Test
    public void testBuild_noEllipsize() {
        ListItem listItem = new ListItemBuilder().build();
        PropertyModel model = listItem.model;

        assertFalse(
                "Ellipsize property should not be present or false in the model if not set",
                model.get(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END));
    }
}
