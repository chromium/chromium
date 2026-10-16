// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxPedal;
import org.chromium.chrome.browser.omnibox.suggestions.action.SiteSearchAction;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.omnibox.AutocompleteInput.SiteSearchData;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.ActionPresentationMode;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.components.omnibox.action.OmniboxPedalId;
import org.chromium.components.search_engines.StarterPackId;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.List;

/** Tests for {@link ActionChipsProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionChipsProcessorUnitTest {
    private static final int MATCH_POS = 1234;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private SuggestionHost mSuggestionHost;
    @Mock private OmniboxActionDelegate mActionDelegate;

    private ActionChipsProcessor mProcessor;
    private PropertyModel mModel;
    private ModelList mActionModel;

    @Before
    public void setUp() {
        mProcessor = new ActionChipsProcessor(mSuggestionHost, mActionDelegate);
        mModel = new PropertyModel(ActionChipsProperties.ALL_UNIQUE_KEYS);
    }

    private OmniboxAction createAction(
            long handle, int textAppearance, @ActionPresentationMode int presentationMode) {
        return new OmniboxAction(
                OmniboxActionId.ACTION_IN_SUGGEST,
                handle,
                "hint",
                "accessibility hint",
                OmniboxAction.DEFAULT_ICON,
                textAppearance,
                presentationMode,
                WindowOpenDisposition.CURRENT_TAB) {
            @Override
            public boolean execute(OmniboxActionDelegate delegate) {
                return true;
            }
        };
    }

    private OmniboxAction actionWithHandleAndTextAppearance(long handle, int textAppearance) {
        return createAction(handle, textAppearance, ActionPresentationMode.CHIP);
    }

    /** Create a suggestion with supplied OmniboxActions (if any) and populate model. */
    private void populateModelForActions(OmniboxAction... actions) {
        AutocompleteMatch match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED)
                        .setActions(List.of(actions))
                        .build();
        mProcessor.populateModel(match, mModel, MATCH_POS);
        mActionModel = mModel.get(ActionChipsProperties.ACTION_CHIPS);
    }

    @Test
    public void populateModel_noActions_setsNull() {
        populateModelForActions();
        assertNull(mActionModel);
    }

    @Test
    public void populateModel_skipsButtonPresentationMode() {
        OmniboxAction buttonAction =
                createAction(1, R.style.TextAppearance_ChipText, ActionPresentationMode.BUTTON);
        populateModelForActions(buttonAction);
        assertNull(mActionModel);

        OmniboxAction chipAction =
                createAction(2, R.style.TextAppearance_ChipText, ActionPresentationMode.CHIP);
        populateModelForActions(buttonAction, chipAction);
        assertNotNull(mActionModel);
        assertEquals(1, mActionModel.size());
    }

    @Test
    public void chipPropertiesAndTextAppearance() {
        populateModelForActions(
                actionWithHandleAndTextAppearance(1, R.style.TextAppearance_ChipText),
                actionWithHandleAndTextAppearance(
                        2, R.style.TextAppearance_TextMediumThick_Primary_Baseline));

        PropertyModel firstChip = mActionModel.get(0).model;
        assertEquals("hint", firstChip.get(ChipProperties.TEXT));
        assertEquals("accessibility hint", firstChip.get(ChipProperties.CONTENT_DESCRIPTION));
        assertTrue(firstChip.get(ChipProperties.ENABLED));
        assertEquals(OmniboxAction.DEFAULT_ICON.chipIconRes, firstChip.get(ChipProperties.ICON));
        assertEquals(
                OmniboxAction.DEFAULT_ICON.tintWithTextColor,
                firstChip.get(ChipProperties.APPLY_ICON_TINT));
        assertEquals(
                R.style.TextAppearance_ChipText,
                firstChip.get(ChipProperties.PRIMARY_TEXT_APPEARANCE));
        assertEquals(
                R.style.TextAppearance_TextMediumThick_Primary_Baseline,
                mActionModel.get(1).model.get(ChipProperties.PRIMARY_TEXT_APPEARANCE));
    }

    @Test
    public void chipClick_invokesSuggestionHostForFirstAndNthAction() {
        OmniboxAction action1 =
                actionWithHandleAndTextAppearance(1, R.style.TextAppearance_ChipText);
        OmniboxAction action2 =
                actionWithHandleAndTextAppearance(2, R.style.TextAppearance_ChipText);
        OmniboxAction action3 =
                actionWithHandleAndTextAppearance(3, R.style.TextAppearance_ChipText);
        populateModelForActions(action1, action2, action3);

        PropertyModel firstChip = mActionModel.get(0).model;
        firstChip.get(ChipProperties.CLICK_HANDLER).onResult(firstChip);
        verify(mSuggestionHost).onOmniboxActionClicked(action1, MATCH_POS);

        PropertyModel thirdChip = mActionModel.get(2).model;
        thirdChip.get(ChipProperties.CLICK_HANDLER).onResult(thirdChip);
        verify(mSuggestionHost).onOmniboxActionClicked(action3, MATCH_POS);
    }

    @Test
    public void chipSelect_notifiesAction() {
        SiteSearchAction action =
                new SiteSearchAction(
                        /* nativeInstance= */ 1,
                        "hint",
                        "accessibility hint",
                        "keyword",
                        StarterPackId.TABS);
        populateModelForActions(action);

        PropertyModel chipModel = mActionModel.get(0).model;
        chipModel.get(ChipProperties.SELECT_HANDLER).onResult(true);
        verify(mActionDelegate)
                .setSiteSearchData(
                        new SiteSearchData(
                                "keyword",
                                "hint",
                                /* enteredViaSpace= */ false,
                                StarterPackId.TABS));
    }

    @Test
    public void populateModel_omniboxPedals() {
        OmniboxPedal clearBrowsingDataPedal =
                new OmniboxPedal(
                        /* nativeInstance= */ 1,
                        "Clear browsing data",
                        "Clear browsing data accessibility",
                        OmniboxPedalId.CLEAR_BROWSING_DATA);
        OmniboxPedal dinoPedal =
                new OmniboxPedal(
                        /* nativeInstance= */ 2,
                        "Play Chrome Dino game",
                        "Play Chrome Dino game accessibility",
                        OmniboxPedalId.PLAY_CHROME_DINO_GAME);
        populateModelForActions(clearBrowsingDataPedal, dinoPedal);

        assertEquals(2, mActionModel.size());

        PropertyModel firstChip = mActionModel.get(0).model;
        assertEquals("Clear browsing data", firstChip.get(ChipProperties.TEXT));
        assertEquals(
                "Clear browsing data accessibility",
                firstChip.get(ChipProperties.CONTENT_DESCRIPTION));
        assertEquals(OmniboxAction.DEFAULT_ICON.chipIconRes, firstChip.get(ChipProperties.ICON));
        firstChip.get(ChipProperties.CLICK_HANDLER).onResult(firstChip);
        verify(mSuggestionHost).onOmniboxActionClicked(clearBrowsingDataPedal, MATCH_POS);

        PropertyModel secondChip = mActionModel.get(1).model;
        assertEquals("Play Chrome Dino game", secondChip.get(ChipProperties.TEXT));
        assertEquals(R.drawable.action_dino_game, secondChip.get(ChipProperties.ICON));
        secondChip.get(ChipProperties.CLICK_HANDLER).onResult(secondChip);
        verify(mSuggestionHost).onOmniboxActionClicked(dinoPedal, MATCH_POS);
    }
}
