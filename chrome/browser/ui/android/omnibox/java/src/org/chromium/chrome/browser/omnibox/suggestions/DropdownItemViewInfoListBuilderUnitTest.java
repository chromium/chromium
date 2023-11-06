// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_1_NO_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_2_WITH_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_3_WITH_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_MOST_VISITED;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.dividerline.DividerLineProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.history_clusters.HistoryClustersProcessor;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto;
import org.chromium.components.omnibox.GroupsProto.GroupConfig;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link DropdownItemViewInfoListBuilder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DropdownItemViewInfoListBuilderUnitTest {
    public @Rule TestRule mProcessor = new Features.JUnitProcessor();
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private @Mock AutocompleteController mAutocompleteController;
    private @Mock SuggestionProcessor mMockSuggestionProcessor;
    private @Mock HeaderProcessor mMockHeaderProcessor;
    private @Mock DividerLineProcessor mMockDividerLineProcessor;
    private @Mock HistoryClustersProcessor.OpenHistoryClustersDelegate mOpenHistoryClustersDelegate;
    DropdownItemViewInfoListBuilder mBuilder;

    @Before
    public void setUp() {
        when(mMockSuggestionProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockSuggestionProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.DEFAULT);

        when(mMockHeaderProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockHeaderProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.HEADER);

        mBuilder =
                new DropdownItemViewInfoListBuilder(
                        () -> null, (url) -> false, mOpenHistoryClustersDelegate);
        mBuilder.registerSuggestionProcessor(mMockSuggestionProcessor);
        mBuilder.setHeaderProcessorForTest(mMockHeaderProcessor);
    }

    /**
     * Verify that two lists have exactly same content. Note: this works similarly to
     * Assert.assertEquals(list1, list2), but instead of printing out the content of both lists,
     * simply reports elements that differ. AutocompleteMatch.toString() is verbose enough that the
     * result analysis may be difficult or even impossible for a small list if the output exceeds
     * the Android's logcat entry length limit.
     */
    private <T> void verifyListsMatch(List<T> expected, List<T> actual) {
        Assert.assertEquals(expected.size(), actual.size());
        for (int index = 0; index < expected.size(); index++) {
            Assert.assertEquals(
                    "Item at position " + index + " does not match",
                    expected.get(index),
                    actual.get(index));
        }
    }

    @Test
    public void buildDropdownViewInfoList_mixedGroups() {
        final var groupsDetails =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(1, SECTION_MOST_VISITED)
                        .putGroupConfigs(2, SECTION_2_WITH_HEADER)
                        .build();

        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);

        AutocompleteMatch horizontal =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();
        AutocompleteMatch vertical =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(2)
                        .build();

        var actualList =
                List.of(horizontal, horizontal, vertical, vertical, horizontal, horizontal);
        var model =
                mBuilder.buildDropdownViewInfoList(
                        AutocompleteResult.fromCache(actualList, groupsDetails));

        // 1 horizontal + 1 header + 2 vertical + 1 horizontal.
        Assert.assertEquals(5, model.size());

        // Check reported positions in list.
        verify(mMockSuggestionProcessor, atLeastOnce()).doesProcessSuggestion(horizontal, 0);
        verify(mMockSuggestionProcessor).doesProcessSuggestion(vertical, 1);
        verify(mMockSuggestionProcessor).doesProcessSuggestion(vertical, 2);
        verify(mMockSuggestionProcessor, atLeastOnce()).doesProcessSuggestion(horizontal, 3);

        verify(mMockSuggestionProcessor, times(2)).populateModel(eq(horizontal), any(), eq(0));
        verify(mMockSuggestionProcessor).populateModel(eq(vertical), any(), eq(1));
        verify(mMockSuggestionProcessor).populateModel(eq(vertical), any(), eq(2));
        verify(mMockSuggestionProcessor, times(2)).populateModel(eq(horizontal), any(), eq(3));

        // Other calls we expect to see.
        verify(mMockSuggestionProcessor).onSuggestionsReceived();
        verify(mMockSuggestionProcessor, times(4)).createModel();
        verify(mMockSuggestionProcessor, atLeastOnce()).getViewTypeId();
        verifyNoMoreInteractions(mMockSuggestionProcessor);
    }

    @Test
    public void headers_buildsHeadersOnlyWhenGroupChanges() {
        final List<AutocompleteMatch> actualList = new ArrayList<>();
        final var groupsDetails =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(1, SECTION_2_WITH_HEADER)
                        .putGroupConfigs(2, SECTION_3_WITH_HEADER)
                        .build();

        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        AutocompleteMatch suggestionWithNoGroup =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        AutocompleteMatch suggestionForGroup1 =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();
        AutocompleteMatch suggestionForGroup2 =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(2)
                        .build();

        actualList.add(suggestionWithNoGroup);
        actualList.add(suggestionForGroup1);
        actualList.add(suggestionForGroup1);
        actualList.add(suggestionForGroup2);
        actualList.add(suggestionForGroup2);

        final InOrder verifier = inOrder(mMockSuggestionProcessor, mMockHeaderProcessor);
        final List<DropdownItemViewInfo> model =
                mBuilder.buildDropdownViewInfoList(
                        AutocompleteResult.fromCache(actualList, groupsDetails));

        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionWithNoGroup), any(), eq(0));
        verifier.verify(mMockHeaderProcessor, times(1))
                .populateModel(any(), eq(SECTION_2_WITH_HEADER.getHeaderText()));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup1), any(), eq(1));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup1), any(), eq(2));
        verifier.verify(mMockHeaderProcessor, times(1))
                .populateModel(any(), eq(SECTION_3_WITH_HEADER.getHeaderText()));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup2), any(), eq(3));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup2), any(), eq(4));
        Assert.assertEquals(7, model.size()); // 2 headers + 5 suggestions.

        var defaultGroupConfig = GroupConfig.getDefaultInstance();

        Assert.assertEquals(model.get(0).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(0).groupConfig, defaultGroupConfig);

        Assert.assertEquals(model.get(1).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(model.get(1).groupConfig, SECTION_2_WITH_HEADER);
        Assert.assertEquals(model.get(2).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(2).groupConfig, SECTION_2_WITH_HEADER);
        Assert.assertEquals(model.get(3).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(3).groupConfig, SECTION_2_WITH_HEADER);

        Assert.assertEquals(model.get(4).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(model.get(4).groupConfig, SECTION_3_WITH_HEADER);
        Assert.assertEquals(model.get(5).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(5).groupConfig, SECTION_3_WITH_HEADER);
        Assert.assertEquals(model.get(6).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(6).groupConfig, SECTION_3_WITH_HEADER);
    }

    @Test
    public void headers_respectGroupHeadersWithNoTitle() {
        final List<AutocompleteMatch> actualList = new ArrayList<>();
        final var groupsDetails =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(1, SECTION_1_NO_HEADER)
                        .putGroupConfigs(2, SECTION_2_WITH_HEADER)
                        .build();

        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        AutocompleteMatch suggestionWithNoGroup =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        AutocompleteMatch suggestionForGroup1 =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();
        AutocompleteMatch suggestionForGroup2 =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(2)
                        .build();

        actualList.add(suggestionWithNoGroup);
        actualList.add(suggestionForGroup1);
        actualList.add(suggestionForGroup1);
        actualList.add(suggestionForGroup2);
        actualList.add(suggestionForGroup2);

        final InOrder verifier = inOrder(mMockSuggestionProcessor, mMockHeaderProcessor);
        final List<DropdownItemViewInfo> model =
                mBuilder.buildDropdownViewInfoList(
                        AutocompleteResult.fromCache(actualList, groupsDetails));

        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionWithNoGroup), any(), eq(0));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup1), any(), eq(1));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup1), any(), eq(2));
        verifier.verify(mMockHeaderProcessor, times(1))
                .populateModel(any(), eq(SECTION_2_WITH_HEADER.getHeaderText()));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup2), any(), eq(3));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup2), any(), eq(4));

        // Make sure no other headers were ever constructed.
        verify(mMockHeaderProcessor, times(1)).populateModel(any(), any());

        var defaultGroupConfig = GroupConfig.getDefaultInstance();

        Assert.assertEquals(6, model.size()); // 1 header + 5 suggestions.

        Assert.assertEquals(model.get(0).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(0).groupConfig, defaultGroupConfig);

        Assert.assertEquals(model.get(1).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(1).groupConfig, SECTION_1_NO_HEADER);
        Assert.assertEquals(model.get(2).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(2).groupConfig, SECTION_1_NO_HEADER);

        Assert.assertEquals(model.get(3).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(model.get(3).groupConfig, SECTION_2_WITH_HEADER);
        Assert.assertEquals(model.get(4).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(4).groupConfig, SECTION_2_WITH_HEADER);
        Assert.assertEquals(model.get(5).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(5).groupConfig, SECTION_2_WITH_HEADER);
    }

    @Test
    public void builder_propagatesOmniboxSessionStateChangeEvents() {
        mBuilder.onOmniboxSessionStateChange(true);
        verify(mMockHeaderProcessor, times(1)).onOmniboxSessionStateChange(eq(true));
        verify(mMockSuggestionProcessor, times(1)).onOmniboxSessionStateChange(eq(true));

        mBuilder.onOmniboxSessionStateChange(false);
        verify(mMockHeaderProcessor, times(1)).onOmniboxSessionStateChange(eq(false));
        verify(mMockSuggestionProcessor, times(1)).onOmniboxSessionStateChange(eq(false));

        verifyNoMoreInteractions(mMockHeaderProcessor);
        verifyNoMoreInteractions(mMockSuggestionProcessor);
    }

    @Test
    public void builder_propagatesNativeInitializedEvent() {
        mBuilder.onNativeInitialized();
        verify(mMockHeaderProcessor, times(1)).onNativeInitialized();
        verify(mMockSuggestionProcessor, times(1)).onNativeInitialized();

        verifyNoMoreInteractions(mMockHeaderProcessor);
        verifyNoMoreInteractions(mMockSuggestionProcessor);
    }

    @Test
    public void visibleSuggestions_missingDropdownHeightAssumesDefaultGroupSize() {
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(AutocompleteMatch.class), anyInt()))
                .thenReturn(true);
        // Create AutocompleteResult with a lot of suggestions.
        final AutocompleteMatch match = builder.build();
        final AutocompleteResult result =
                AutocompleteResult.fromCache(
                        Arrays.asList(
                                match, match, match, match, match, match, match, match, match,
                                match),
                        null);
        Assert.assertEquals(5, mBuilder.getVisibleSuggestionsCount(result));

        // Same, with a shorter list of suggestions; in this case we don't know the height of the
        // dropdown view, so we assume we can comfortably fit 5 suggestions.
        final AutocompleteResult shortResult =
                AutocompleteResult.fromCache(
                        Arrays.asList(match, match, match, match, match), null);
        Assert.assertEquals(5, mBuilder.getVisibleSuggestionsCount(shortResult));
    }

    @Test
    public void visibleSuggestions_computeNumberOfVisibleSuggestionsFromDropdownHeight() {
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(AutocompleteMatch.class), anyInt()))
                .thenReturn(true);
        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);

        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        final AutocompleteMatch match = builder.build();
        final AutocompleteResult result =
                AutocompleteResult.fromCache(
                        Arrays.asList(
                                match, match, match, match, match, match, match, match, match,
                                match),
                        null);

        mBuilder.setDropdownHeightWithKeyboardActive(60);
        Assert.assertEquals(6, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(90);
        Assert.assertEquals(9, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(30);
        Assert.assertEquals(3, mBuilder.getVisibleSuggestionsCount(result));
    }

    @Test
    public void visibleSuggestions_partiallyVisibleSuggestionsAreCountedAsVisible() {
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(AutocompleteMatch.class), anyInt()))
                .thenReturn(true);
        final AutocompleteMatch match = builder.build();
        final AutocompleteResult result =
                AutocompleteResult.fromCache(
                        Arrays.asList(
                                match, match, match, match, match, match, match, match, match,
                                match),
                        null);

        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);
        mBuilder.setDropdownHeightWithKeyboardActive(45);
        Assert.assertEquals(5, mBuilder.getVisibleSuggestionsCount(result));

        // 40% of the next suggestion exposed - still not sufficient to be considered "visible".
        mBuilder.setDropdownHeightWithKeyboardActive(54);
        Assert.assertEquals(5, mBuilder.getVisibleSuggestionsCount(result));

        // 50% of the next suggestion exposed - considered "visible".
        mBuilder.setDropdownHeightWithKeyboardActive(55);
        Assert.assertEquals(6, mBuilder.getVisibleSuggestionsCount(result));
    }

    @Test
    public void visibleSuggestions_queriesCorrespondingProcessorsToDetermineViewAllocation() {
        final SuggestionProcessor mockProcessor1 = mock(SuggestionProcessor.class);
        final SuggestionProcessor mockProcessor2 = mock(SuggestionProcessor.class);
        mBuilder.registerSuggestionProcessor(mockProcessor1);
        mBuilder.registerSuggestionProcessor(mockProcessor2);
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        final AutocompleteMatch match1 = builder.setDescription("1").build();
        final AutocompleteMatch match2 = builder.setDescription("2").build();
        final AutocompleteMatch match3 = builder.setDescription("3").build();
        final AutocompleteResult result =
                AutocompleteResult.fromCache(Arrays.asList(match1, match2, match3), null);

        // Heights reported by processors for suggestions 1, 2 and 3.
        when(mMockSuggestionProcessor.doesProcessSuggestion(eq(match1), anyInt())).thenReturn(true);
        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);
        when(mockProcessor1.doesProcessSuggestion(eq(match2), anyInt())).thenReturn(true);
        when(mockProcessor1.getMinimumViewHeight()).thenReturn(20);
        when(mockProcessor2.doesProcessSuggestion(eq(match3), anyInt())).thenReturn(true);
        when(mockProcessor2.getMinimumViewHeight()).thenReturn(30);

        mBuilder.setDropdownHeightWithKeyboardActive(
                90); // fits all three suggestions and then some.
        Assert.assertEquals(3, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(
                45); // fits 2 suggestions fully, and 3rd partially.
        Assert.assertEquals(3, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(30); // fits only 2 suggestions.
        Assert.assertEquals(2, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(
                20); // fits one suggestion fully and one partially.
        Assert.assertEquals(2, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(10); // fits only one suggestion.
        Assert.assertEquals(1, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(5); // fits one suggestion partiall.
        Assert.assertEquals(1, mBuilder.getVisibleSuggestionsCount(result));
    }

    @Test
    public void performPartialGroupingBySearchVsUrl_noActionWithNoMatches() {
        AutocompleteResult mockResult = mock(AutocompleteResult.class);
        when(mockResult.getSuggestionsList()).thenReturn(Arrays.asList());
        mBuilder.performPartialGroupingBySearchVsUrl(mockResult);
        verify(mockResult).getSuggestionsList();
        verifyNoMoreInteractions(mockResult);
    }

    @Test
    public void performPartialGroupingBySearchVsUrl_noActionWithTooFewMatches() {
        // Adaptive Suggestions needs a Default match (always first, cannot be moved) and
        // at least two more that need to be grouped or rearranged.
        final AutocompleteMatch match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        AutocompleteResult mockResult = mock(AutocompleteResult.class);
        when(mockResult.getSuggestionsList()).thenReturn(Arrays.asList(match, match));
        mBuilder.performPartialGroupingBySearchVsUrl(mockResult);
        verify(mockResult).getSuggestionsList();
        verifyNoMoreInteractions(mockResult);
    }

    @Test
    public void performPartialGroupingBySearchVsUrl_groupSuggestionsAboveFold() {
        // Adaptive Suggestions needs a Default match (always first, cannot be moved) and
        // at least two more that need to be grouped or rearranged.
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);
        mBuilder.setDropdownHeightWithKeyboardActive(50);

        final AutocompleteMatch search =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        AutocompleteResult mockResult = mock(AutocompleteResult.class);
        when(mockResult.getSuggestionsList()).thenReturn(Arrays.asList(search, search, search));
        mBuilder.performPartialGroupingBySearchVsUrl(mockResult);
        verify(mockResult, atLeastOnce()).getSuggestionsList();
        verify(mockResult).groupSuggestionsBySearchVsURL(1, 3);
        verifyNoMoreInteractions(mockResult);
    }

    @Test
    public void performPartialGroupingBySearchVsUrl_groupSuggestionsAboveAndBelowFold() {
        // Adaptive Suggestions needs a Default match (always first, cannot be moved) and
        // at least two more that need to be grouped or rearranged.
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);
        mBuilder.setDropdownHeightWithKeyboardActive(30);

        final AutocompleteMatch search =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        AutocompleteResult mockResult = mock(AutocompleteResult.class);
        when(mockResult.getSuggestionsList())
                .thenReturn(Arrays.asList(search, search, search, search, search));
        mBuilder.performPartialGroupingBySearchVsUrl(mockResult);
        verify(mockResult, atLeastOnce()).getSuggestionsList();
        verify(mockResult).groupSuggestionsBySearchVsURL(1, 3);
        verify(mockResult).groupSuggestionsBySearchVsURL(3, 5);
        verifyNoMoreInteractions(mockResult);
    }

    @Test
    public void performPartialGroupingBySearchVsUrl_matchesWithHeaderAreNotPromotedAboveURLs() {
        final SuggestionProcessor mockProcessor = mock(SuggestionProcessor.class);
        mBuilder.registerSuggestionProcessor(mockProcessor);
        final AutocompleteMatch match1 =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        final AutocompleteMatch match2 =
                new AutocompleteMatchBuilder(OmniboxSuggestionType.NAVSUGGEST).build();
        final AutocompleteMatch match3 =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();

        // Simulate 6 suggestions by repeating the three above.
        AutocompleteResult mockResult = mock(AutocompleteResult.class);
        when(mockResult.getSuggestionsList())
                .thenReturn(Arrays.asList(match1, match2, match1, match2, match3, match3));
        var groupsInfo = GroupsInfo.newBuilder();
        groupsInfo.putGroupConfigs(1, GroupsProto.GroupConfig.getDefaultInstance());
        when(mockResult.getGroupsInfo()).thenReturn(groupsInfo.build());
        doNothing().when(mockResult).groupSuggestionsBySearchVsURL(anyInt(), anyInt());

        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);

        // Scenario 1: everything fits above keyboard. Last 2 suggestions are not touched.
        mBuilder.setDropdownHeightWithKeyboardActive(90);
        mBuilder.buildDropdownViewInfoList(mockResult);
        verify(mockResult, times(1)).groupSuggestionsBySearchVsURL(1, 4);
        verify(mockResult, times(1)).groupSuggestionsBySearchVsURL(anyInt(), anyInt());
        clearInvocations(mockResult);

        // Scenario 2: Suggestions to group fit just above the keyboard
        mBuilder.setDropdownHeightWithKeyboardActive(35);
        mBuilder.buildDropdownViewInfoList(mockResult);
        verify(mockResult, times(1)).groupSuggestionsBySearchVsURL(1, 4);
        verify(mockResult, times(1)).groupSuggestionsBySearchVsURL(anyInt(), anyInt());
        clearInvocations(mockResult);

        // Scenario 3a: Some suggestions to group fit above the keyboard
        mBuilder.setDropdownHeightWithKeyboardActive(25);
        mBuilder.buildDropdownViewInfoList(mockResult);
        verify(mockResult, times(1)).groupSuggestionsBySearchVsURL(1, 3);
        verify(mockResult, times(1)).groupSuggestionsBySearchVsURL(3, 4);
        verify(mockResult, times(2)).groupSuggestionsBySearchVsURL(anyInt(), anyInt());
        clearInvocations(mockResult);

        // Scenario 3b: Some suggestions to group fit above the keyboard
        mBuilder.setDropdownHeightWithKeyboardActive(15);
        mBuilder.buildDropdownViewInfoList(mockResult);
        verify(mockResult, times(1)).groupSuggestionsBySearchVsURL(1, 2);
        verify(mockResult, times(1)).groupSuggestionsBySearchVsURL(2, 4);
        verify(mockResult, times(2)).groupSuggestionsBySearchVsURL(anyInt(), anyInt());
        clearInvocations(mockResult);

        // Skipping scenario where all suggestions are below the keyboard, because in this scenario
        // the user can't realistically interact with them.
    }

    @Test
    public void buildVerticalSuggestionsGroup_withoutGroupHeader() {
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();
        var matches = List.of(match, match);
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        clearInvocations(mMockHeaderProcessor, mMockSuggestionProcessor);

        var result =
                mBuilder.buildVerticalSuggestionsGroup(
                        SECTION_1_NO_HEADER, matches, /* firstVerticalPosition= */ 5);

        verify(mMockSuggestionProcessor, times(2)).createModel();
        verify(mMockSuggestionProcessor, atLeastOnce()).getViewTypeId();
        verify(mMockSuggestionProcessor).doesProcessSuggestion(match, 5);
        verify(mMockSuggestionProcessor).populateModel(eq(match), any(), eq(5));
        verify(mMockSuggestionProcessor).doesProcessSuggestion(match, 6);
        verify(mMockSuggestionProcessor).populateModel(eq(match), any(), eq(6));

        verifyNoMoreInteractions(mMockHeaderProcessor, mMockSuggestionProcessor);

        assertEquals(/* 0 header + 2 suggestions = */ 2, result.size());
    }

    @Test
    public void buildVerticalSuggestionsGroup_withGroupHeader() {
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();
        var matches = List.of(match, match);
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        clearInvocations(mMockHeaderProcessor, mMockSuggestionProcessor);

        var result =
                mBuilder.buildVerticalSuggestionsGroup(
                        SECTION_2_WITH_HEADER, matches, /* firstVerticalPosition= */ 7);

        verify(mMockHeaderProcessor).createModel();
        verify(mMockHeaderProcessor, atLeastOnce()).getViewTypeId();
        verify(mMockHeaderProcessor)
                .populateModel(any(), eq(SECTION_2_WITH_HEADER.getHeaderText()));

        verify(mMockSuggestionProcessor, times(2)).createModel();
        verify(mMockSuggestionProcessor, atLeastOnce()).getViewTypeId();
        verify(mMockSuggestionProcessor).doesProcessSuggestion(match, 7);
        verify(mMockSuggestionProcessor).populateModel(eq(match), any(), eq(7));
        verify(mMockSuggestionProcessor).doesProcessSuggestion(match, 8);
        verify(mMockSuggestionProcessor).populateModel(eq(match), any(), eq(8));

        verifyNoMoreInteractions(mMockHeaderProcessor, mMockSuggestionProcessor);

        assertEquals(/* 1 header + 2 suggestions = */ 3, result.size());
    }

    @Test
    public void buildHorizontalSuggestionsGroup_withoutGroupHeader() {
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();
        var matches = List.of(match, match);
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        clearInvocations(mMockHeaderProcessor, mMockSuggestionProcessor);

        var result =
                mBuilder.buildHorizontalSuggestionsGroup(
                        SECTION_1_NO_HEADER, matches, /* firstVerticalPosition= */ 5);

        var captor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMockSuggestionProcessor).getViewTypeId();
        verify(mMockSuggestionProcessor).createModel();
        verify(mMockSuggestionProcessor, atLeastOnce()).doesProcessSuggestion(match, 5);
        verify(mMockSuggestionProcessor, times(2))
                .populateModel(eq(match), captor.capture(), eq(5));
        verifyNoMoreInteractions(mMockHeaderProcessor, mMockSuggestionProcessor);

        assertEquals(/* 0 header + 1 suggestion row = */ 1, result.size());

        // Verify that the same PropertyModel was used to build UI element, and it's the one that
        // was returned.
        assertEquals(captor.getAllValues().get(0), captor.getAllValues().get(1));
        assertEquals(captor.getValue(), result.get(0).model);
    }

    @Test
    public void buildHorizontalSuggestionsGroup_withGroupHeader() {
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();
        var matches = List.of(match, match);
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        clearInvocations(mMockHeaderProcessor, mMockSuggestionProcessor);

        var result =
                mBuilder.buildHorizontalSuggestionsGroup(
                        SECTION_2_WITH_HEADER, matches, /* firstVerticalPosition= */ 7);

        verify(mMockHeaderProcessor).createModel();
        verify(mMockHeaderProcessor, atLeastOnce()).getViewTypeId();
        verify(mMockHeaderProcessor)
                .populateModel(any(), eq(SECTION_2_WITH_HEADER.getHeaderText()));

        var captor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMockSuggestionProcessor).getViewTypeId();
        verify(mMockSuggestionProcessor).createModel();
        verify(mMockSuggestionProcessor, atLeastOnce()).doesProcessSuggestion(match, 7);
        verify(mMockSuggestionProcessor, times(2))
                .populateModel(eq(match), captor.capture(), eq(7));

        verifyNoMoreInteractions(mMockHeaderProcessor, mMockSuggestionProcessor);

        assertEquals(/* 1 header + 1 suggestion row = */ 2, result.size());

        // Verify that the same PropertyModel was used to build UI element, and it's the one that
        // was returned.
        assertEquals(captor.getAllValues().get(0), captor.getAllValues().get(1));
        assertEquals(captor.getValue(), result.get(1).model);
    }

    @Test
    public void dividerLineOnTop() {
        when(mMockDividerLineProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockDividerLineProcessor.getViewTypeId())
                .thenReturn(OmniboxSuggestionUiType.DIVIDER_LINE);
        mBuilder.setDividerLineProcessorForTest(mMockDividerLineProcessor);

        final List<AutocompleteMatch> actualList = new ArrayList<>();
        final var groupsDetails =
                GroupsInfo.newBuilder().putGroupConfigs(1, SECTION_2_WITH_HEADER).build();
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);

        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();

        actualList.add(suggestion);
        actualList.add(suggestion);

        final List<DropdownItemViewInfo> infoList =
                mBuilder.buildDropdownViewInfoList(
                        AutocompleteResult.fromCache(actualList, groupsDetails));
        Assert.assertEquals(4, infoList.size()); // 1 divider line + 1 header + 2 suggestions.

        Assert.assertEquals(infoList.get(0).type, OmniboxSuggestionUiType.DIVIDER_LINE);
        Assert.assertEquals(infoList.get(1).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(infoList.get(2).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(infoList.get(3).type, OmniboxSuggestionUiType.DEFAULT);

        mBuilder.setDividerLineProcessorForTest(null);
    }

    @Test
    public void noDividerLineForEmptyList() {
        when(mMockDividerLineProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockDividerLineProcessor.getViewTypeId())
                .thenReturn(OmniboxSuggestionUiType.DIVIDER_LINE);
        mBuilder.setDividerLineProcessorForTest(mMockDividerLineProcessor);

        final List<DropdownItemViewInfo> infoList =
                mBuilder.buildDropdownViewInfoList(AutocompleteResult.fromCache(null, null));
        Assert.assertEquals(0, infoList.size());

        mBuilder.setDividerLineProcessorForTest(null);
    }

    @Test
    public void visibleSuggestions_updatedVisibleGroupEligibilityLogic() {
        final SuggestionProcessor mockProcessor = mock(SuggestionProcessor.class);
        mBuilder.registerSuggestionProcessor(mockProcessor);
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        final AutocompleteMatch match1 = builder.setDescription("1").build();
        final AutocompleteMatch match2 = builder.setDescription("2").build();
        final AutocompleteResult result =
                AutocompleteResult.fromCache(Arrays.asList(match1, match2), null);

        // Each suggestion is 20dp tall, asking 40dp total space.
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(20);

        // Given 40dp area, both suggestions should be fully exposed.
        mBuilder.setDropdownHeightWithKeyboardActive(40);
        Assert.assertEquals(2, mBuilder.getVisibleSuggestionsCount(result));

        // Given 30dp area, both suggestions should be still considered fully visible:
        // One suggestion is 100% exposed, the other is 50% exposed
        mBuilder.setDropdownHeightWithKeyboardActive(30);
        Assert.assertEquals(2, mBuilder.getVisibleSuggestionsCount(result));

        // Given 29dp area, one of the suggestions is no longer considered exposed.
        // 9dp is less than 50% of the 20dp it needs.
        mBuilder.setDropdownHeightWithKeyboardActive(29);
        Assert.assertEquals(1, mBuilder.getVisibleSuggestionsCount(result));

        // Given 20dp area, one of the suggestions is fully concealed.
        mBuilder.setDropdownHeightWithKeyboardActive(20);
        Assert.assertEquals(1, mBuilder.getVisibleSuggestionsCount(result));

        // Given 10dp area, one of the suggestions is still considered exposed,
        // while the other is fully conealed. This is because 10dp is 50% of required 20dp.
        mBuilder.setDropdownHeightWithKeyboardActive(10);
        Assert.assertEquals(1, mBuilder.getVisibleSuggestionsCount(result));

        // Given 9dp area, none of the suggestions are considered visible.
        // There's not enough space to show even one of them.
        mBuilder.setDropdownHeightWithKeyboardActive(9);
        Assert.assertEquals(0, mBuilder.getVisibleSuggestionsCount(result));
    }
}
