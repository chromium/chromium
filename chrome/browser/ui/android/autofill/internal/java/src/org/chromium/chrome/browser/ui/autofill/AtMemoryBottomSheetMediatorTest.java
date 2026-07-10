// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SearchItemProperties.ON_TILE_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SearchItemProperties.TILE_TITLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties.IS_FLYOUT_VISIBLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties.ON_FLYOUT_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties.ON_SUGGESTION_CLICKED;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties.TITLE;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.personal_context.first_run.PersonalContextFirstRunService;
import org.chromium.chrome.browser.personal_context.first_run.PersonalContextFirstRunServiceJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.FlyoutProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.HomeProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ScreenId;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SearchItemProperties;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link AtMemoryBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class AtMemoryBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AtMemoryBottomSheetCoordinator.Delegate mDelegate;
    @Mock private SearchItemProperties.Delegate mSearchDelegate;
    @Mock private Profile mProfile;
    @Mock private PersonalContextFirstRunService.Natives mFirstRunServiceJniMock;

    private PropertyModel mModel;
    private PropertyModel mHomeModel;
    private ModelList mModelList;
    private AtMemoryBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        PersonalContextFirstRunServiceJni.setInstanceForTesting(mFirstRunServiceJniMock);
        mMediator =
                new AtMemoryBottomSheetMediator(
                        ApplicationProvider.getApplicationContext(),
                        mProfile,
                        mDelegate,
                        mSearchDelegate);
        mModel = mMediator.getModel();
        mHomeModel = mMediator.getHomeModel();
        mModelList = mHomeModel.get(HomeProperties.SHEET_ITEMS);
    }

    @Test
    public void testOnSuggestionClicked() {
        List<AutofillSuggestion> suggestions =
                List.of(
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.flight)
                                .setLabel("KLM204")
                                .setSubLabel("Flight ⋅ 15 May ⋅ SEA - MUC")
                                .build(),
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.travel_trip)
                                .setLabel("Hotel Booking")
                                .setSubLabel("Hilton ⋅ 16 May")
                                .build());

        mMediator.show(suggestions);

        assertTrue(mModel.get(VISIBLE));
        assertEquals(2, mModelList.size());

        assertEquals(suggestions.get(0).getLabel(), mModelList.get(0).model.get(TITLE));
        assertEquals(suggestions.get(1).getLabel(), mModelList.get(1).model.get(TITLE));

        PropertyModel itemModel1 = mModelList.get(0).model;
        itemModel1.get(ON_SUGGESTION_CLICKED).run();

        verify(mDelegate).onSuggestionClicked(/* position= */ 0);
    }

    @Test
    public void testOnFlyoutClicked() {
        AutofillSuggestion childSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Hilton Check-in")
                        .setSubLabel("May 16")
                        .build();
        List<AutofillSuggestion> suggestions =
                List.of(
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.flight)
                                .setLabel("KLM204")
                                .setSubLabel("Flight ⋅ 15 May ⋅ SEA - MUC")
                                .build(),
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.travel_trip)
                                .setLabel("Hotel Booking")
                                .setSubLabel("Hilton ⋅ 16 May")
                                .setChildren(List.of(childSuggestion))
                                .build());

        mMediator.show(suggestions);

        PropertyModel itemModel2 = mModelList.get(1).model;
        itemModel2.get(ON_FLYOUT_CLICKED).run();

        PropertyModel flyoutModel = mMediator.getFlyoutModel();
        assertEquals("Hotel Booking", flyoutModel.get(FlyoutProperties.TITLE));
        assertEquals(List.of(childSuggestion), flyoutModel.get(FlyoutProperties.SUGGESTIONS));
    }

    @Test
    public void testFlyoutVisible() {
        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setIconId(R.drawable.flight)
                        .setLabel("KLM204")
                        .setSubLabel("Flight ⋅ 15 May ⋅ SEA - MUC")
                        .setSuggestionType(SuggestionType.AT_MEMORY_SEARCH_RESULT)
                        .build();
        List<AutofillSuggestion> suggestions = List.of(suggestion);
        mMediator.show(suggestions);
        assertTrue(mModelList.get(0).model.get(IS_FLYOUT_VISIBLE));
    }

    @Test
    public void testFlyoutNotVisible() {
        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setIconId(R.drawable.sad_tab)
                        .setLabel("Recent")
                        .setSubLabel("No connection")
                        .setSuggestionType(SuggestionType.AT_MEMORY_NO_CONNECTION)
                        .build();
        List<AutofillSuggestion> suggestions = List.of(suggestion);
        mMediator.show(suggestions);
        assertFalse(mModelList.get(0).model.get(IS_FLYOUT_VISIBLE));
    }

    @Test
    public void testOnFlyoutClickedTriggersDelegate() {
        AutofillSuggestion childSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Hilton Check-in")
                        .setSubLabel("May 16")
                        .build();
        List<AutofillSuggestion> suggestions =
                List.of(
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.travel_trip)
                                .setLabel("Hotel Booking")
                                .setSubLabel("Hilton ⋅ 16 May")
                                .setChildren(List.of(childSuggestion))
                                .build());

        mMediator.show(suggestions);

        PropertyModel itemModel = mModelList.get(0).model;
        itemModel.get(ON_FLYOUT_CLICKED).run();

        verify(mDelegate).onChildSuggestionsShown(/* parentPosition= */ 0);
    }

    @Test
    public void testOnFlyoutSuggestionClicked() {
        AutofillSuggestion childSuggestion0 =
                new AutofillSuggestion.Builder()
                        .setLabel("Hilton Check-in")
                        .setSubLabel("May 16")
                        .build();
        AutofillSuggestion childSuggestion1 =
                new AutofillSuggestion.Builder()
                        .setLabel("Hilton Checkout")
                        .setSubLabel("May 20")
                        .build();
        List<AutofillSuggestion> suggestions =
                List.of(
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.flight)
                                .setLabel("KLM204")
                                .setSubLabel("Flight ⋅ 15 May ⋅ SEA - MUC")
                                .build(),
                        new AutofillSuggestion.Builder()
                                .setIconId(R.drawable.travel_trip)
                                .setLabel("Hotel Booking")
                                .setSubLabel("Hilton ⋅ 16 May")
                                .setChildren(List.of(childSuggestion0, childSuggestion1))
                                .build());

        mMediator.show(suggestions);

        PropertyModel itemModel = mModelList.get(1).model;
        itemModel.get(ON_FLYOUT_CLICKED).run();

        PropertyModel flyoutModel = mMediator.getFlyoutModel();
        flyoutModel.get(FlyoutProperties.ON_SUGGESTION_CLICKED).onResult(1);

        verify(mDelegate).onChildSuggestionClicked(1, 1);
    }

    @Test
    public void testOnDismissed() {
        mModel.set(VISIBLE, true);
        mHomeModel.set(HomeProperties.IS_LOADING, true);

        PropertyModel flyoutModel = mMediator.getFlyoutModel();
        flyoutModel.set(FlyoutProperties.TITLE, "Title");

        mMediator.onDismissed();

        assertFalse(mModel.get(VISIBLE));
        assertFalse(mHomeModel.get(HomeProperties.IS_LOADING));
        assertEquals(ScreenId.HOME_SCREEN, mModel.get(CURRENT_SCREEN));

        assertEquals("", flyoutModel.get(FlyoutProperties.TITLE));
        assertEquals(List.of(), flyoutModel.get(FlyoutProperties.SUGGESTIONS));

        verify(mDelegate).onDismissed();
    }

    @Test
    public void testOnQuerySubmitted() {
        mMediator.onQuerySubmitted("flight");
        assertTrue(mHomeModel.get(HomeProperties.IS_LOADING));
        verify(mDelegate).onQuerySubmitted("flight");

        when(mDelegate.isSearching()).thenReturn(true);
        mMediator.show(List.of());
        assertTrue(mHomeModel.get(HomeProperties.IS_LOADING));

        when(mDelegate.isSearching()).thenReturn(false);
        mMediator.show(
                List.of(
                        new AutofillSuggestion.Builder()
                                .setLabel("No data")
                                .setSubLabel("")
                                .build()));
        assertFalse(mHomeModel.get(HomeProperties.IS_LOADING));
    }

    @Test
    public void testOnQueryTextChanged() {
        mHomeModel.get(HomeProperties.SEARCH_BAR_DELEGATE).onQueryTextChanged("flight");
        verify(mDelegate).onQueryTextChanged("flight");

        mMediator.show(List.of(createSearchAffordance("flight")));
        assertEquals(1, mModelList.size());
        assertEquals(HomeProperties.ItemType.SEARCH_TILE, mModelList.get(0).type);
        assertEquals("flight", mModelList.get(0).model.get(TILE_TITLE));
        assertFalse(mHomeModel.get(HomeProperties.SHOW_SUGGESTIONS_BACKGROUND));

        mModelList.get(0).model.get(ON_TILE_CLICKED).run();
        verify(mSearchDelegate).hideKeyboardAndClearFocus();
        verify(mDelegate).onQuerySubmitted("flight");
    }

    @Test
    public void testOnQueryTextChanged_subsequentKeystrokes() {
        mMediator.show(List.of(createSearchAffordance("f")));
        assertEquals(1, mModelList.size());
        ListItem firstItem = mModelList.get(0);
        assertEquals("f", firstItem.model.get(TILE_TITLE));

        mMediator.show(List.of(createSearchAffordance("fl")));
        assertEquals(1, mModelList.size());
        assertTrue(firstItem == mModelList.get(0));
        assertEquals("fl", firstItem.model.get(TILE_TITLE));
    }

    @Test
    public void testOnQueryTextChanged_emptyQueryShowsZeroState() {
        mMediator.show(List.of(createSearchAffordance("f")));
        assertEquals(1, mModelList.size());

        mMediator.show(List.of());
        assertEquals(1, mModelList.size());
        assertEquals(HomeProperties.ItemType.ZERO_STATE, mModelList.get(0).type);
    }

    private AutofillSuggestion createSearchAffordance(String query) {
        return new AutofillSuggestion.Builder()
                .setLabel(query)
                .setSubLabel("test details")
                .setIconId(R.drawable.flight)
                .setSuggestionType(SuggestionType.AT_MEMORY_SEARCH_AFFORDANCE)
                .build();
    }

    @Test
    public void testShow_emptySuggestionsShowsZeroState() {
        mMediator.show(List.of());

        assertEquals(1, mModelList.size());
        assertEquals(HomeProperties.ItemType.ZERO_STATE, mModelList.get(0).type);
    }

    @Test
    public void testNoticeShownAndDismissedAfterClick() {
        when(mFirstRunServiceJniMock.shouldShowAtMemoryNotice(mProfile)).thenReturn(true);

        HistogramWatcher shownWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AtMemoryBottomSheetMediator.NOTICE_INTERACTIONS_HISTOGRAM,
                        AtMemoryBottomSheetMediator.NoticeInteraction.SHOWN);

        AtMemoryBottomSheetMediator mediator =
                new AtMemoryBottomSheetMediator(
                        ApplicationProvider.getApplicationContext(),
                        mProfile,
                        mDelegate,
                        mSearchDelegate);
        PropertyModel homeModel = mediator.getHomeModel();

        assertTrue(homeModel.get(HomeProperties.IS_NOTICE_VISIBLE));

        mediator.show(List.of());
        shownWatcher.assertExpected();

        HistogramWatcher acknowledgedWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AtMemoryBottomSheetMediator.NOTICE_INTERACTIONS_HISTOGRAM,
                        AtMemoryBottomSheetMediator.NoticeInteraction.ACKNOWLEDGED);

        Runnable okClickListener = homeModel.get(HomeProperties.NOTICE_OK_CLICK_LISTENER);
        assertNotNull(okClickListener);
        okClickListener.run();

        assertFalse(homeModel.get(HomeProperties.IS_NOTICE_VISIBLE));
        acknowledgedWatcher.assertExpected();
        verify(mFirstRunServiceJniMock).atMemoryNoticeAcknowledged(mProfile);
    }

    @Test
    public void testNoticeSettingsClicked() {
        UserActionTester userActionTester = new UserActionTester();
        AtMemoryBottomSheetMediator mediator =
                new AtMemoryBottomSheetMediator(
                        ApplicationProvider.getApplicationContext(),
                        mProfile,
                        mDelegate,
                        mSearchDelegate);
        PropertyModel homeModel = mediator.getHomeModel();

        Runnable settingsClickListener =
                homeModel.get(HomeProperties.NOTICE_SETTINGS_CLICK_LISTENER);
        assertNotNull(settingsClickListener);
        settingsClickListener.run();

        assertTrue(
                userActionTester
                        .getActions()
                        .contains("PersonalContext.AtMemory.Notice.SettingsLinkClick"));
    }

    @Test
    public void testNoticeNotShown() {
        when(mFirstRunServiceJniMock.shouldShowAtMemoryNotice(mProfile)).thenReturn(false);

        AtMemoryBottomSheetMediator mediator =
                new AtMemoryBottomSheetMediator(
                        ApplicationProvider.getApplicationContext(),
                        mProfile,
                        mDelegate,
                        mSearchDelegate);

        assertFalse(mediator.getHomeModel().get(HomeProperties.IS_NOTICE_VISIBLE));
    }

    @Test
    public void testNoticeShownRecordedOnlyOnce() {
        when(mFirstRunServiceJniMock.shouldShowAtMemoryNotice(mProfile)).thenReturn(true);

        HistogramWatcher shownWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AtMemoryBottomSheetMediator.NOTICE_INTERACTIONS_HISTOGRAM,
                                AtMemoryBottomSheetMediator.NoticeInteraction.SHOWN)
                        .build();

        AtMemoryBottomSheetMediator mediator =
                new AtMemoryBottomSheetMediator(
                        ApplicationProvider.getApplicationContext(),
                        mProfile,
                        mDelegate,
                        mSearchDelegate);

        mediator.show(List.of());
        mediator.show(List.of()); // Second call should not log again.

        shownWatcher.assertExpected();
    }

    @Test
    public void testOnSearchFocus() {
        mHomeModel.get(HomeProperties.SEARCH_BAR_DELEGATE).onSearchFocus(true);
        verify(mDelegate).onSearchFocus(true);
    }
}
