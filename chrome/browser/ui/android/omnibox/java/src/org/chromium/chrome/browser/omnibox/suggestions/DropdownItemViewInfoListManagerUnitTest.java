// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.Mockito.when;

import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_1_NO_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_2_WITH_HEADER;

import android.content.Context;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link DropdownItemViewInfoListManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DropdownItemViewInfoListManagerUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

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

        mSuggestionModels = new ModelList();
        mSuggestionModels.addObserver(mListObserver);

        mContext = ContextUtils.getApplicationContext();
        mManager = new DropdownItemViewInfoListManager(mSuggestionModels, mContext);
        mManager.onNativeInitialized();
    }

    /**
     * Verify that the content of the resulting Suggestions list matches the supplied list. Asserts
     * if the two lists differ.
     */
    private void verifyModelEquals(List<DropdownItemViewInfo> expected) {
        Assert.assertEquals(expected.size(), mSuggestionModels.size());

        for (int index = 0; index < expected.size(); index++) {
            Assert.assertEquals(
                    "Element at position " + index + " does not match",
                    expected.get(index),
                    mSuggestionModels.get(index));
        }
    }

    /**
     * Verify that PropertyModels of all suggestions on managed list reflect the expected values.
     */
    private void verifyPropertyValues(
            int layoutDirection, @BrandedColorScheme int brandedColorScheme) {
        for (int index = 0; index < mSuggestionModels.size(); index++) {
            Assert.assertEquals(
                    "Unexpected layout direction for suggestion at position " + index,
                    layoutDirection,
                    mSuggestionModels
                            .get(index)
                            .model
                            .get(SuggestionCommonProperties.LAYOUT_DIRECTION));
            Assert.assertEquals(
                    "Unexpected visual theme for suggestion at position " + index,
                    brandedColorScheme,
                    mSuggestionModels
                            .get(index)
                            .model
                            .get(SuggestionCommonProperties.COLOR_SCHEME));
        }
    }

    @Test
    public void updateSuggestionsList_suggestionsAreRebuiltOnSubsequentInteractions() {
        // This test validates scenario:
        // 1. user focuses omnibox
        // 2. AutocompleteMediator receives suggestions
        // 3. user sees suggestions, but leaves omnibox
        // 4. user focuses omnibox again
        // 5. AutocompleteMediator receives same suggestions as in (2)
        // 6. user sees suggestions again.
        final List<DropdownItemViewInfo> list1 = new ArrayList<>();
        list1.add(new DropdownItemViewInfo(mHeaderProcessor, mModel, SECTION_1_NO_HEADER));
        list1.add(new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, SECTION_1_NO_HEADER));
        list1.add(new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, SECTION_1_NO_HEADER));

        final List<DropdownItemViewInfo> list2 =
                Arrays.asList(
                        new DropdownItemViewInfo(mHeaderProcessor, mModel, SECTION_1_NO_HEADER),
                        new DropdownItemViewInfo(
                                mBasicSuggestionProcessor, mModel, SECTION_1_NO_HEADER),
                        new DropdownItemViewInfo(
                                mBasicSuggestionProcessor, mModel, SECTION_1_NO_HEADER));

        mManager.setSourceViewInfoList(list1);
        verifyModelEquals(list1);

        mManager.clear();

        mManager.setSourceViewInfoList(list2);
        verifyModelEquals(list2);
    }

    @Test
    public void updateSuggestionsList_uiChangesArePropagatedToSuggestions() {
        List<DropdownItemViewInfo> list =
                Arrays.asList(
                        new DropdownItemViewInfo(
                                mHeaderProcessor,
                                new PropertyModel(SuggestionCommonProperties.ALL_KEYS),
                                SECTION_1_NO_HEADER),
                        new DropdownItemViewInfo(
                                mBasicSuggestionProcessor,
                                new PropertyModel(SuggestionCommonProperties.ALL_KEYS),
                                SECTION_1_NO_HEADER),
                        new DropdownItemViewInfo(
                                mBasicSuggestionProcessor,
                                new PropertyModel(SuggestionCommonProperties.ALL_KEYS),
                                SECTION_1_NO_HEADER));

        mManager.setSourceViewInfoList(list);
        verifyModelEquals(list);
        verifyPropertyValues(View.LAYOUT_DIRECTION_INHERIT, BrandedColorScheme.LIGHT_BRANDED_THEME);

        mManager.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, BrandedColorScheme.LIGHT_BRANDED_THEME);

        mManager.setBrandedColorScheme(BrandedColorScheme.DARK_BRANDED_THEME);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, BrandedColorScheme.DARK_BRANDED_THEME);

        mManager.setBrandedColorScheme(BrandedColorScheme.INCOGNITO);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, BrandedColorScheme.INCOGNITO);

        // Finally, set the new list and confirm that the values are still applied.
        list =
                Arrays.asList(
                        new DropdownItemViewInfo(
                                mHeaderProcessor,
                                new PropertyModel(SuggestionCommonProperties.ALL_KEYS),
                                SECTION_2_WITH_HEADER),
                        new DropdownItemViewInfo(
                                mBasicSuggestionProcessor,
                                new PropertyModel(SuggestionCommonProperties.ALL_KEYS),
                                SECTION_2_WITH_HEADER),
                        new DropdownItemViewInfo(
                                mBasicSuggestionProcessor,
                                new PropertyModel(SuggestionCommonProperties.ALL_KEYS),
                                SECTION_2_WITH_HEADER),
                        new DropdownItemViewInfo(
                                mBasicSuggestionProcessor,
                                new PropertyModel(SuggestionCommonProperties.ALL_KEYS),
                                SECTION_2_WITH_HEADER));
        mManager.setSourceViewInfoList(list);
        verifyModelEquals(list);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, BrandedColorScheme.INCOGNITO);
    }
}
