// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_1_COLLAPSED_NO_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_1_EXPANDED_NO_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_2_COLLAPSED_WITH_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_2_EXPANDED_WITH_HEADER;

import android.content.Context;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.ShadowGURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link DropdownItemViewInfoListManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGURL.class})
@Features.EnableFeatures({ChromeFeatureList.OMNIBOX_MODERNIZE_VISUAL_UPDATE})
public class DropdownItemViewInfoListManagerUnitTest {
    private static final int MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW = 5;
    private static final int SUGGESTION_MIN_HEIGHT = 20;
    private static final int HEADER_MIN_HEIGHT = 15;

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    public @Rule TestRule mProcessor = new Features.JUnitProcessor();

    private @Spy SuggestionProcessor mBasicSuggestionProcessor;
    private @Spy SuggestionProcessor mEditUrlSuggestionProcessor;
    private @Spy DropdownItemProcessor mHeaderProcessor;
    private @Mock PropertyModel mModel;
    private @Mock ListObserver<Void> mListObserver;

    private ModelList mSuggestionModels;
    private Context mContext;
    private DropdownItemViewInfoListManager mManager;

    @Before
    public void setUp() {
        when(mBasicSuggestionProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.DEFAULT);
        when(mEditUrlSuggestionProcessor.getViewTypeId())
                .thenReturn(OmniboxSuggestionUiType.EDIT_URL_SUGGESTION);
        when(mHeaderProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.HEADER);
        ChromeFeatureList.sOmniboxModernizeVisualUpdate.setForTesting(true);

        mSuggestionModels = new ModelList();
        mSuggestionModels.addObserver(mListObserver);

        mContext = ContextUtils.getApplicationContext();
        mManager = new DropdownItemViewInfoListManager(mSuggestionModels, mContext);
        mManager.onNativeInitialized();

        Assert.assertTrue(OmniboxFeatures.shouldShowModernizeVisualUpdate(mContext));
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.resetFlagsForTesting();
    }

    /**
     * Verify that the content of the resulting Suggestions list matches the supplied list.
     * Asserts if the two lists differ.
     */
    private void verifyModelEquals(List<DropdownItemViewInfo> expected) {
        Assert.assertEquals(expected.size(), mSuggestionModels.size());

        for (int index = 0; index < expected.size(); index++) {
            Assert.assertEquals("Element at position " + index + " does not match",
                    expected.get(index), mSuggestionModels.get(index));
        }
    }

    /**
     * Verify that PropertyModels of all suggestions on managed list reflect the expected values.
     */
    private void verifyPropertyValues(
            int layoutDirection, @BrandedColorScheme int brandedColorScheme) {
        for (int index = 0; index < mSuggestionModels.size(); index++) {
            Assert.assertEquals("Unexpected layout direction for suggestion at position " + index,
                    layoutDirection,
                    mSuggestionModels.get(index).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
            Assert.assertEquals("Unexpected visual theme for suggestion at position " + index,
                    brandedColorScheme,
                    mSuggestionModels.get(index).model.get(
                            SuggestionCommonProperties.COLOR_SCHEME));
        }
    }

    @Test
    @SmallTest
    public void modelUpdates_visibilityChangesOnlyUpdateTheModel() {
        // This change confirms that we do not re-create entire model, but instead insert/remove
        // items that have been added/removed from the list.
        final List<DropdownItemViewInfo> list = Arrays.asList(
                // Group 1: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                // Group 2: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2));

        mManager.setSourceViewInfoList(list, GroupsInfo.newBuilder().build());
        verifyModelEquals(list);

        // Monitor updates moving forward.
        reset(mListObserver);

        // Collapse group 1.
        mManager.setGroupCollapsedState(1, true);
        verify(mListObserver, times(1)).onItemRangeRemoved(any(), eq(1), eq(2));
        // Collapse group 2.
        mManager.setGroupCollapsedState(2, true);
        verify(mListObserver, times(1)).onItemRangeRemoved(any(), eq(2), eq(2));

        // Expand group 1.
        mManager.setGroupCollapsedState(1, false);
        verify(mListObserver, times(1)).onItemRangeInserted(any(), eq(1), eq(2));

        // Expand group 2.
        mManager.setGroupCollapsedState(2, false);
        verify(mListObserver, times(1)).onItemRangeInserted(any(), eq(4), eq(2));

        verifyNoMoreInteractions(mListObserver);
    }

    @Test
    @SmallTest
    public void groupHandling_toggleGroupExpandedState() {
        final List<DropdownItemViewInfo> listWithBothGroupsExpanded = Arrays.asList(
                // Group 1: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                // Group 2: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2));

        mManager.setSourceViewInfoList(listWithBothGroupsExpanded, GroupsInfo.newBuilder().build());
        verifyModelEquals(listWithBothGroupsExpanded);

        // Toggle group 1.
        final List<DropdownItemViewInfo> listWithGroup1Collapsed = new ArrayList<>();
        listWithGroup1Collapsed.add(listWithBothGroupsExpanded.get(0));
        listWithGroup1Collapsed.addAll(listWithBothGroupsExpanded.subList(3, 6));
        mManager.setGroupCollapsedState(1, true);
        verifyModelEquals(listWithGroup1Collapsed);
        mManager.setGroupCollapsedState(1, false);
        verifyModelEquals(listWithBothGroupsExpanded);

        // Toggle group 2.
        final List<DropdownItemViewInfo> listWithGroup2Collapsed =
                listWithBothGroupsExpanded.subList(0, 4);
        mManager.setGroupCollapsedState(2, true);
        verifyModelEquals(listWithGroup2Collapsed);
        mManager.setGroupCollapsedState(2, false);
        verifyModelEquals(listWithBothGroupsExpanded);

        // Toggle both groups.
        final List<DropdownItemViewInfo> listWithBothGroupsCollapsed =
                listWithGroup1Collapsed.subList(0, 2);
        mManager.setGroupCollapsedState(2, true);
        verifyModelEquals(listWithGroup2Collapsed);
        mManager.setGroupCollapsedState(1, true);
        verifyModelEquals(listWithBothGroupsCollapsed);
        mManager.setGroupCollapsedState(2, false);
        verifyModelEquals(listWithGroup1Collapsed);
        mManager.setGroupCollapsedState(1, false);
        verifyModelEquals(listWithBothGroupsExpanded);
    }

    @Test
    @SmallTest
    public void groupHandling_defaultGroupExpandedState() {
        final List<DropdownItemViewInfo> listWithBothGroupsExpanded = Arrays.asList(
                // Group 1: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                // Group 2: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2));

        // Receive suggestions list with group 1 default-collapsed.
        mManager.setSourceViewInfoList(listWithBothGroupsExpanded,
                GroupsInfo.newBuilder()
                        .putGroupConfigs(1, SECTION_1_COLLAPSED_NO_HEADER)
                        .putGroupConfigs(2, SECTION_2_EXPANDED_WITH_HEADER)
                        .build());

        final List<DropdownItemViewInfo> listWithGroup1Collapsed = new ArrayList<>();
        listWithGroup1Collapsed.add(listWithBothGroupsExpanded.get(0));
        listWithGroup1Collapsed.addAll(listWithBothGroupsExpanded.subList(3, 6));
        verifyModelEquals(listWithGroup1Collapsed);

        // Expand suggestions for group 1.
        mManager.setGroupCollapsedState(1, false);
        verifyModelEquals(listWithBothGroupsExpanded);

        // Receive suggestions list with group 2 default-collapsed.
        mManager.setSourceViewInfoList(listWithBothGroupsExpanded,
                GroupsInfo.newBuilder()
                        .putGroupConfigs(1, SECTION_1_EXPANDED_NO_HEADER)
                        .putGroupConfigs(2, SECTION_2_COLLAPSED_WITH_HEADER)
                        .build());
        final List<DropdownItemViewInfo> listWithGroup2Collapsed =
                listWithBothGroupsExpanded.subList(0, 4);
        verifyModelEquals(listWithGroup2Collapsed);
        // Expand suggestions for group 2.
        mManager.setGroupCollapsedState(2, false);
        verifyModelEquals(listWithBothGroupsExpanded);

        // Receive suggestions list with both groups default-collapsed.
        mManager.setSourceViewInfoList(listWithBothGroupsExpanded,
                GroupsInfo.newBuilder()
                        .putGroupConfigs(1, SECTION_1_COLLAPSED_NO_HEADER)
                        .putGroupConfigs(2, SECTION_2_COLLAPSED_WITH_HEADER)
                        .build());
        final List<DropdownItemViewInfo> listWithBothGroupsCollapsed =
                listWithGroup1Collapsed.subList(0, 2);
        verifyModelEquals(listWithBothGroupsCollapsed);
        mManager.setGroupCollapsedState(2, false);
        verifyModelEquals(listWithGroup1Collapsed);
        mManager.setGroupCollapsedState(1, false);
        verifyModelEquals(listWithBothGroupsExpanded);
    }

    @Test
    @SmallTest
    public void groupHandling_expandingAlreadyExpandedGroupAddsNoNewElementns() {
        final List<DropdownItemViewInfo> list = Arrays.asList(
                // Group 1: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                // Group 2: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2));

        mManager.setSourceViewInfoList(list, GroupsInfo.newBuilder().build());
        verifyModelEquals(list);

        // Expand group 1.
        mManager.setGroupCollapsedState(1, false);
        verifyModelEquals(list);

        // Expand group 2.
        mManager.setGroupCollapsedState(2, false);
        verifyModelEquals(list);
    }

    @Test
    @SmallTest
    public void groupHandling_collapseAlreadyCollapsedListIsNoOp() {
        final List<DropdownItemViewInfo> list = Arrays.asList(
                // Group 1: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                // Group 2: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2));

        mManager.setSourceViewInfoList(list, GroupsInfo.newBuilder().build());
        verifyModelEquals(list);

        // Collapse group 1.
        final List<DropdownItemViewInfo> listWithGroup1Collapsed = new ArrayList<>();
        listWithGroup1Collapsed.add(list.get(0));
        listWithGroup1Collapsed.addAll(list.subList(3, 6));
        mManager.setGroupCollapsedState(1, true);
        verifyModelEquals(listWithGroup1Collapsed);
        mManager.setGroupCollapsedState(1, true);
        verifyModelEquals(listWithGroup1Collapsed);

        // Collapse group 2.
        final List<DropdownItemViewInfo> listWithBothGroupsCollapsed =
                listWithGroup1Collapsed.subList(0, 2);
        mManager.setGroupCollapsedState(2, true);
        verifyModelEquals(listWithBothGroupsCollapsed);
        mManager.setGroupCollapsedState(2, true);
        verifyModelEquals(listWithBothGroupsCollapsed);
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_suggestionsAreRebuiltOnSubsequentInteractions() {
        // This test validates scenario:
        // 1. user focuses omnibox
        // 2. AutocompleteMediator receives suggestions
        // 3. user sees suggestions, but leaves omnibox
        // 4. user focuses omnibox again
        // 5. AutocompleteMediator receives same suggestions as in (2)
        // 6. user sees suggestions again.
        final List<DropdownItemViewInfo> list1 = new ArrayList<>();
        list1.add(new DropdownItemViewInfo(mHeaderProcessor, mModel, 1));
        list1.add(new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1));
        list1.add(new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1));

        final List<DropdownItemViewInfo> list2 =
                Arrays.asList(new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                        new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                        new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1));

        mManager.setSourceViewInfoList(list1, GroupsInfo.newBuilder().build());
        verifyModelEquals(list1);

        mManager.clear();

        mManager.setSourceViewInfoList(list2, GroupsInfo.newBuilder().build());
        verifyModelEquals(list2);
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_uiChangesArePropagatedToSuggestions() {
        List<DropdownItemViewInfo> list =
                Arrays.asList(new DropdownItemViewInfo(mHeaderProcessor,
                                      new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 1),
                        new DropdownItemViewInfo(mBasicSuggestionProcessor,
                                new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 1),
                        new DropdownItemViewInfo(mBasicSuggestionProcessor,
                                new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 1));

        mManager.setSourceViewInfoList(list, GroupsInfo.newBuilder().build());
        verifyModelEquals(list);
        verifyPropertyValues(View.LAYOUT_DIRECTION_INHERIT, BrandedColorScheme.LIGHT_BRANDED_THEME);

        mManager.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, BrandedColorScheme.LIGHT_BRANDED_THEME);

        mManager.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, BrandedColorScheme.DARK_BRANDED_THEME);

        mManager.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, BrandedColorScheme.INCOGNITO);

        // Finally, set the new list and confirm that the values are still applied.
        list = Arrays.asList(new DropdownItemViewInfo(mHeaderProcessor,
                                     new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 2));
        mManager.setSourceViewInfoList(list, GroupsInfo.newBuilder().build());
        verifyModelEquals(list);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, BrandedColorScheme.INCOGNITO);
    }

    @Test
    @SmallTest
    public void suggestionsListRoundedCorners() {
        final int groupIdNoHeader = 1;
        final int groupIdWithHeader = 2;

        List<DropdownItemViewInfo> list = Arrays.asList(
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdNoHeader),
                new DropdownItemViewInfo(mHeaderProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader));

        mManager.setSourceViewInfoList(list,
                GroupsInfo.newBuilder()
                        .putGroupConfigs(groupIdNoHeader, SECTION_1_EXPANDED_NO_HEADER)
                        .putGroupConfigs(groupIdWithHeader, SECTION_2_EXPANDED_WITH_HEADER)
                        .build());
        verifyModelEquals(list);

        //
        // ******************** <--- rounded corner
        // * basic suggestion *
        // ******************** <--- rounded corner
        //                      <--- no corner
        //  header suggestion
        //                      <--- no corner
        // ******************** <--- rounded corner
        // * basic suggestion *
        // ******************** <--- sharped corner
        // ******************** <--- sharped corner
        // * basic suggestion *
        // ******************** <--- rounded corner
        //
        Assert.assertTrue(
                mSuggestionModels.get(0).model.get(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED));
        Assert.assertTrue(mSuggestionModels.get(0).model.get(
                DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED));
        Assert.assertFalse(
                mSuggestionModels.get(1).model.get(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED));
        Assert.assertFalse(mSuggestionModels.get(1).model.get(
                DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED));
        Assert.assertTrue(
                mSuggestionModels.get(2).model.get(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED));
        Assert.assertFalse(mSuggestionModels.get(2).model.get(
                DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED));
        Assert.assertFalse(
                mSuggestionModels.get(3).model.get(DropdownCommonProperties.BG_TOP_CORNER_ROUNDED));
        Assert.assertTrue(mSuggestionModels.get(3).model.get(
                DropdownCommonProperties.BG_BOTTOM_CORNER_ROUNDED));
    }

    @Test
    @SmallTest
    public void suggestionsListSpacing_NonActiveOmnibox_bigBottomMargin() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX.setForTesting(false);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN.setForTesting(false);

        int suggestionListTopMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_list_non_active_top_big_margin);
        int groupVerticalSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_group_vertical_margin);
        int suggestionVerticalSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_vertical_margin);

        final int groupIdNoHeader = 1;
        final int groupIdWithHeader = 2;

        List<DropdownItemViewInfo> list = Arrays.asList(
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdNoHeader),
                new DropdownItemViewInfo(mHeaderProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader));

        // Receive suggestions list with group 1 default-collapsed.
        mManager.setSourceViewInfoList(list,
                GroupsInfo.newBuilder()
                        .putGroupConfigs(groupIdNoHeader, SECTION_1_EXPANDED_NO_HEADER)
                        .putGroupConfigs(groupIdWithHeader, SECTION_2_EXPANDED_WITH_HEADER)
                        .build());
        verifyModelEquals(list);

        //
        // ******************** <--- very first one, non active Omnibox big margin.
        // * basic suggestion *
        // ******************** <--- last one in a group, group margin.
        //                      <--- no background, no margin
        //  header suggestion
        //                      <--- no background, no margin
        // ******************** <--- first one in a group, group margin.
        // * basic suggestion *
        // ******************** <--- normal bottom, no margin.
        // ******************** <--- normal top, suggestion margin.
        // * basic suggestion *
        // ******************** <--- very last one, no margin.
        //
        Assert.assertEquals(suggestionListTopMargin,
                mSuggestionModels.get(0).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(groupVerticalSpacing,
                mSuggestionModels.get(0).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(1).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(1).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(groupVerticalSpacing,
                mSuggestionModels.get(2).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(suggestionVerticalSpacing,
                mSuggestionModels.get(2).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(suggestionVerticalSpacing,
                mSuggestionModels.get(3).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(3).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
    }

    @Test
    @SmallTest
    public void suggestionsListSpacing_NonActiveOmnibox_smallBottomMargin() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX.setForTesting(false);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN.setForTesting(true);

        int suggestionListTopMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_list_non_active_top_small_margin);
        int groupTopSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_group_vertical_margin);
        int groupBottomSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_group_vertical_small_bottom_margin);
        int suggestionVerticalSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_vertical_margin);

        final int groupIdNoHeader = 1;
        final int groupIdWithHeader = 2;

        List<DropdownItemViewInfo> list = Arrays.asList(
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdNoHeader),
                new DropdownItemViewInfo(mHeaderProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader));

        // Receive suggestions list with group 1 default-collapsed.
        mManager.setSourceViewInfoList(list,
                GroupsInfo.newBuilder()
                        .putGroupConfigs(groupIdNoHeader, SECTION_1_EXPANDED_NO_HEADER)
                        .putGroupConfigs(groupIdWithHeader, SECTION_2_EXPANDED_WITH_HEADER)
                        .build());
        verifyModelEquals(list);

        //
        // ******************** <--- very first one, non active Omnibox small margin.
        // * basic suggestion *
        // ******************** <--- last one in a group, group margin.
        //                      <--- no background, no margin
        //  header suggestion
        //                      <--- no background, no margin
        // ******************** <--- first one in a group, group margin.
        // * basic suggestion *
        // ******************** <--- normal bottom, no margin.
        // ******************** <--- normal top, suggestion margin.
        // * basic suggestion *
        // ******************** <--- very last one, no margin.
        //
        Assert.assertEquals(suggestionListTopMargin,
                mSuggestionModels.get(0).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(groupBottomSpacing,
                mSuggestionModels.get(0).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(1).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(1).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(groupTopSpacing,
                mSuggestionModels.get(2).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(suggestionVerticalSpacing,
                mSuggestionModels.get(2).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(suggestionVerticalSpacing,
                mSuggestionModels.get(3).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(3).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
    }

    @Test
    @SmallTest
    public void suggestionsListSpacing_activeOmnibox_bigBottomMargin() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN.setForTesting(false);

        int suggestionListTopMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_list_active_top_big_margin);
        int groupVerticalSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_group_vertical_margin);
        int suggestionVerticalSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_vertical_margin);

        final int groupIdNoHeader = 1;
        final int groupIdWithHeader = 2;

        List<DropdownItemViewInfo> list = Arrays.asList(
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdNoHeader),
                new DropdownItemViewInfo(mHeaderProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader));

        // Receive suggestions list with group 1 default-collapsed.
        mManager.setSourceViewInfoList(list,
                GroupsInfo.newBuilder()
                        .putGroupConfigs(groupIdNoHeader, SECTION_1_EXPANDED_NO_HEADER)
                        .putGroupConfigs(groupIdWithHeader, SECTION_2_EXPANDED_WITH_HEADER)
                        .build());
        verifyModelEquals(list);

        //
        // ******************** <--- very first one, active Omnibox big margin.
        // * basic suggestion *
        // ******************** <--- last one in a group, group margin.
        //                      <--- no background, no margin
        //  header suggestion
        //                      <--- no background, no margin
        // ******************** <--- first one in a group, group margin.
        // * basic suggestion *
        // ******************** <--- normal bottom, no margin.
        // ******************** <--- normal top, suggestion margin.
        // * basic suggestion *
        // ******************** <--- very last one, no margin.
        //
        Assert.assertEquals(suggestionListTopMargin,
                mSuggestionModels.get(0).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(groupVerticalSpacing,
                mSuggestionModels.get(0).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(1).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(1).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(groupVerticalSpacing,
                mSuggestionModels.get(2).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(suggestionVerticalSpacing,
                mSuggestionModels.get(2).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(suggestionVerticalSpacing,
                mSuggestionModels.get(3).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(3).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
    }

    @Test
    @SmallTest
    public void suggestionsListSpacing_activeOmnibox_smallBottomMargin() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN.setForTesting(true);

        int suggestionListTopMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_list_active_top_small_margin);
        int groupTopSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_group_vertical_margin);
        int groupBottomSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_group_vertical_small_bottom_margin);
        int suggestionVerticalSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_vertical_margin);

        final int groupIdNoHeader = 1;
        final int groupIdWithHeader = 2;

        List<DropdownItemViewInfo> list = Arrays.asList(
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdNoHeader),
                new DropdownItemViewInfo(mHeaderProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader));

        // Receive suggestions list with group 1 default-collapsed.
        mManager.setSourceViewInfoList(list,
                GroupsInfo.newBuilder()
                        .putGroupConfigs(groupIdNoHeader, SECTION_1_EXPANDED_NO_HEADER)
                        .putGroupConfigs(groupIdWithHeader, SECTION_2_EXPANDED_WITH_HEADER)
                        .build());
        verifyModelEquals(list);

        //
        // ******************** <--- very first one, active Omnibox small margin.
        // * basic suggestion *
        // ******************** <--- last one in a group, group margin.
        //                      <--- no background, no margin
        //  header suggestion
        //                      <--- no background, no margin
        // ******************** <--- first one in a group, group margin.
        // * basic suggestion *
        // ******************** <--- normal bottom, no margin.
        // ******************** <--- normal top, suggestion margin.
        // * basic suggestion *
        // ******************** <--- very last one, no margin.
        //
        Assert.assertEquals(suggestionListTopMargin,
                mSuggestionModels.get(0).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(groupBottomSpacing,
                mSuggestionModels.get(0).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(1).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(1).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(groupTopSpacing,
                mSuggestionModels.get(2).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(suggestionVerticalSpacing,
                mSuggestionModels.get(2).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(suggestionVerticalSpacing,
                mSuggestionModels.get(3).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(3).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
    }

    @Test
    @SmallTest
    public void suggestionsListSpacing_SRO_shouldNoTopMargin() {
        OmniboxFeatures.ENABLE_MODERNIZE_VISUAL_UPDATE_ON_TABLET.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_ACTIVE_COLOR_ON_OMNIBOX.setForTesting(true);
        OmniboxFeatures.MODERNIZE_VISUAL_UPDATE_SMALL_BOTTOM_MARGIN.setForTesting(true);

        int suggestionListTopMargin = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_list_active_top_small_margin);
        int groupTopSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_group_vertical_margin);
        int groupBottomSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_group_vertical_small_bottom_margin);
        int suggestionVerticalSpacing = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_vertical_margin);

        final int groupIdNoHeader = 1;
        final int groupIdWithHeader = 2;

        List<DropdownItemViewInfo> list = Arrays.asList(
                new DropdownItemViewInfo(mEditUrlSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdNoHeader),
                new DropdownItemViewInfo(mHeaderProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), groupIdWithHeader));

        // Receive suggestions list with group 1 default-collapsed.
        mManager.setSourceViewInfoList(list,
                GroupsInfo.newBuilder()
                        .putGroupConfigs(groupIdNoHeader, SECTION_1_EXPANDED_NO_HEADER)
                        .putGroupConfigs(groupIdWithHeader, SECTION_2_EXPANDED_WITH_HEADER)
                        .build());
        verifyModelEquals(list);

        //
        // ******************** <--- very first one, no margin.
        // * basic suggestion *
        // ******************** <--- last one in a group, group margin.
        //                      <--- no background, no margin
        //  header suggestion
        //                      <--- no background, no margin
        // ******************** <--- first one in a group, group margin.
        // * basic suggestion *
        // ******************** <--- normal bottom, no margin.
        // ******************** <--- normal top, suggestion margin.
        // * basic suggestion *
        // ******************** <--- very last one, no margin.
        //
        Assert.assertEquals(
                0, mSuggestionModels.get(0).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(groupBottomSpacing,
                mSuggestionModels.get(0).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(1).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(1).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(groupTopSpacing,
                mSuggestionModels.get(2).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(suggestionVerticalSpacing,
                mSuggestionModels.get(2).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
        Assert.assertEquals(suggestionVerticalSpacing,
                mSuggestionModels.get(3).model.get(DropdownCommonProperties.TOP_MARGIN));
        Assert.assertEquals(
                0, mSuggestionModels.get(3).model.get(DropdownCommonProperties.BOTTOM_MARGIN));
    }
}
