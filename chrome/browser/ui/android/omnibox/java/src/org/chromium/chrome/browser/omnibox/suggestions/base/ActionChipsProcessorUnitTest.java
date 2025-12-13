// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;

/** Tests for {@link ActionChipsProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionChipsProcessorUnitTest {
    private static final int MATCH_POS = 1234;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock SuggestionHost mSuggestionHost;

    private ActionChipsProcessor mProcessor;
    private PropertyModel mModel;
    private ModelList mActionModel;

    @Before
    public void setUp() {
        mProcessor = new ActionChipsProcessor(mSuggestionHost);
        mModel = new PropertyModel(ActionChipsProperties.ALL_UNIQUE_KEYS);
    }

    private OmniboxAction actionWithHandleAndTextAppearance(long handle, int textAppearance) {
        return new OmniboxAction(
                OmniboxActionId.ACTION_IN_SUGGEST,
                handle,
                "hint",
                "accessibility hint",
                OmniboxAction.DEFAULT_ICON,
                textAppearance,
                /* showAsActionButton= */ false,
                WindowOpenDisposition.CURRENT_TAB) {
            @Override
            public void execute(OmniboxActionDelegate delegate) {}
        };
    }

    /** Create a suggestion with supplied OmniboxActions (if any) and populate model. */
    private void populateModelForActions(OmniboxAction... actions) {
        var match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED)
                        .setActions(List.of(actions))
                        .build();
        mProcessor.populateModel(match, mModel, MATCH_POS);
        mActionModel = mModel.get(ActionChipsProperties.ACTION_CHIPS);
    }

    @Test
    public void chipTextAppearance() {
        populateModelForActions(
                actionWithHandleAndTextAppearance(1, R.style.TextAppearance_ChipText),
                actionWithHandleAndTextAppearance(
                        2, R.style.TextAppearance_TextMediumThick_Primary_Baseline));

        ModelList chipModel = mModel.get(ActionChipsProperties.ACTION_CHIPS);
        assertEquals(
                R.style.TextAppearance_ChipText,
                chipModel.get(0).model.get(ChipProperties.PRIMARY_TEXT_APPEARANCE));
        assertEquals(
                R.style.TextAppearance_TextMediumThick_Primary_Baseline,
                chipModel.get(1).model.get(ChipProperties.PRIMARY_TEXT_APPEARANCE));
    }
}
