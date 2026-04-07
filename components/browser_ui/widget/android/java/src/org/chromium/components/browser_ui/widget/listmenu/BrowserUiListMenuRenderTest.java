// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.listmenu;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static org.chromium.components.browser_ui.widget.ListItemBuilder.buildSimpleMenuItem;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.ListView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.MethodRule;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.MethodParamAnnotationRule;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Render tests for {@link BasicListMenu}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.UNIT_TESTS)
public class BrowserUiListMenuRenderTest {
    /** Used to run a test only with night mode. */
    public static class NightModeOnlyParameterProvider implements ParameterProvider {

        private static final List<ParameterSet> sNightModeOnly =
                Collections.singletonList(new ParameterSet().value(true).name("NightModeEnabled"));

        @Override
        public Iterable<ParameterSet> getParameters() {
            return sNightModeOnly;
        }
    }

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE)
                    .setRevision(2)
                    .build();

    @Rule public MethodRule mMethodParamAnnotationProcessor = new MethodParamAnnotationRule();

    private View mView;

    private void setup(ModelList data, boolean nightMode, boolean incognito) {
        Activity activity = mActivityTestRule.launchActivity(null);
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightMode);
        mRenderTestRule.setNightModeEnabled(nightMode);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ListMenu.Delegate delegate = (item, view) -> {};
                    BasicListMenu listMenu =
                            BrowserUiListMenuUtils.getBasicListMenu(activity, data, delegate);
                    listMenu.setupCallbacksRecursively(
                            /* dismissDialog= */ () -> {},
                            ListMenuUtils.createHierarchicalMenuController(activity));

                    mView = listMenu.getContentView();
                    mView.setBackground(
                            AppCompatResources.getDrawable(activity, R.drawable.menu_bg_tinted));
                    int width =
                            activity.getResources().getDimensionPixelSize(R.dimen.list_menu_width);
                    activity.setContentView(mView, new LayoutParams(width, WRAP_CONTENT));
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @After
    public void tearDown() throws Exception {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @UseMethodParameter(NightModeParams.class)
    public void testRender_BasicListMenu(boolean nightMode) throws IOException {
        setup(
                getModelListWithoutSubmenu(/* incognito= */ false),
                nightMode,
                /* incognito= */ false);
        mRenderTestRule.render(mView, "basic_list_menu");
    }

    @Test
    @MediumTest
    @UseMethodParameter(NightModeOnlyParameterProvider.class)
    @Feature({"RenderTest"})
    public void testRender_BasicListMenu_Incognito(boolean nightMode) throws IOException {
        setup(getModelListWithoutSubmenu(/* incognito= */ true), nightMode, /* incognito= */ true);
        mRenderTestRule.render(mView, "basic_list_menu_incognito");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @UseMethodParameter(NightModeParams.class)
    public void testRender_BasicListMenu_SubmenuScroll(boolean nightMode) throws IOException {
        setup(getModelListWithSubmenu(/* incognito= */ false), nightMode, /* incognito= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Make the view small so the content will become scrollable.
                    mActivityTestRule
                            .getActivity()
                            .setContentView(mView, new LayoutParams(500, 500));
                    ListView contentView = mView.findViewById(R.id.menu_list);
                    ListItem item = (ListItem) contentView.getItemAtPosition(0);
                    item.model.get(CLICK_LISTENER).onClick(mView);
                    contentView.scrollListBy(5);
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(mView, "basic_list_menu_submenu_scroll");
    }

    private static List<ListItem> getListItems(boolean incognito) {
        List<ListItem> listItems = new ArrayList<>();
        listItems.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.test_primary_1)
                        .withStartIconRes(R.drawable.ic_check_googblue_24dp)
                        .build());
        listItems.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.test_primary_1)
                        .withStartIconRes(R.drawable.ic_check_googblue_24dp)
                        .withEnabled(false)
                        .build());
        listItems.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.test_primary_1)
                        .withEndIconRes(R.drawable.ic_check_googblue_24dp)
                        .build());
        listItems.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.test_primary_1)
                        .withEndIconRes(R.drawable.ic_check_googblue_24dp)
                        .withEnabled(false)
                        .build());
        listItems.add(buildSimpleMenuItem(R.string.test_primary_1));
        listItems.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.test_primary_1)
                        .withEnabled(false)
                        .build());
        listItems.add(BasicListMenu.buildMenuDivider(incognito));
        return listItems;
    }

    private static ModelList getModelListWithoutSubmenu(boolean incognito) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ModelList data = new ModelList();
                    for (ListItem listItem : getListItems(incognito)) {
                        data.add(listItem);
                    }
                    return data;
                });
    }

    private static ModelList getModelListWithSubmenu(boolean incognito) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ModelList data = new ModelList();
                    List<ListItem> listItems = getListItems(incognito);
                    data.add(
                            new ListItem(
                                    MENU_ITEM_WITH_SUBMENU,
                                    new PropertyModel.Builder(
                                                    ListMenuSubmenuItemProperties.ALL_KEYS)
                                            .with(TITLE, "test_label")
                                            .with(SUBMENU_ITEMS, listItems)
                                            .build()));
                    return data;
                });
    }
}
