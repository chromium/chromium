// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.personal_context.first_run.PersonalContextFirstRunService;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.FlyoutProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.HomeProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ScreenId;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SearchItemProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Contains the business logic for the AtMemoryBottomSheet. */
@NullMarked
class AtMemoryBottomSheetMediator implements AtMemorySearchBarView.Delegate {
    static final String NOTICE_INTERACTIONS_HISTOGRAM =
            "PersonalContext.AtMemory.NoticeInteractions";

    // Interactions with the AtMemory notice.
    // LINT.IfChange(NoticeInteraction)
    @IntDef({NoticeInteraction.SHOWN, NoticeInteraction.ACKNOWLEDGED, NoticeInteraction.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    @interface NoticeInteraction {
        int SHOWN = 0;
        int ACKNOWLEDGED = 1;
        int COUNT = 2;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/personal_context/enums.xml:PersonalContextAtMemoryNoticeInteractions)

    private final Context mContext;
    private final Profile mProfile;
    private final PropertyModel mModel;
    private final PropertyModel mHomeModel;
    private final PropertyModel mFlyoutModel;
    private final AtMemoryBottomSheetCoordinator.Delegate mDelegate;
    private final SearchItemProperties.Delegate mSearchDelegate;

    private boolean mWasNoticeShownRecorded;

    AtMemoryBottomSheetMediator(
            Context context,
            Profile profile,
            AtMemoryBottomSheetCoordinator.Delegate delegate,
            SearchItemProperties.Delegate searchDelegate) {
        mContext = context;
        mProfile = profile;
        mDelegate = delegate;
        mSearchDelegate = searchDelegate;

        mModel = createModel();
        mHomeModel = createHomeModel();
        mFlyoutModel = createFlyoutModel();
    }

    PropertyModel getModel() {
        return mModel;
    }

    PropertyModel getHomeModel() {
        return mHomeModel;
    }

    PropertyModel getFlyoutModel() {
        return mFlyoutModel;
    }

    void show(List<AutofillSuggestion> suggestions) {
        applyScreenState(getScreenState(suggestions), suggestions);
        mModel.set(VISIBLE, true);

        if (mHomeModel.get(HomeProperties.IS_NOTICE_VISIBLE) && !mWasNoticeShownRecorded) {
            RecordHistogram.recordEnumeratedHistogram(
                    NOTICE_INTERACTIONS_HISTOGRAM,
                    NoticeInteraction.SHOWN,
                    NoticeInteraction.COUNT);
            mWasNoticeShownRecorded = true;
        }
    }

    void onDismissed() {
        applyScreenState(AtMemoryScreenState.HIDDEN, List.of());

        mDelegate.onDismissed();
    }

    private void onNoticeAcknowledged() {
        mHomeModel.set(HomeProperties.IS_NOTICE_VISIBLE, false);
        RecordHistogram.recordEnumeratedHistogram(
                NOTICE_INTERACTIONS_HISTOGRAM,
                NoticeInteraction.ACKNOWLEDGED,
                NoticeInteraction.COUNT);
        PersonalContextFirstRunService.atMemoryNoticeAcknowledged(mProfile);
    }

    private void onNoticeSettingsClicked() {
        RecordUserAction.record("PersonalContext.AtMemory.Notice.SettingsLinkClick");
        SettingsNavigationFactory.createSettingsNavigation().startSettings(mContext);
    }

    private AtMemoryScreenState getScreenState(List<AutofillSuggestion> suggestions) {
        if (isSearchAffordance(suggestions)) {
            return AtMemoryScreenState.SEARCH_AFFORDANCE;
        }
        if (suggestions.isEmpty()) {
            return mDelegate.isSearching()
                    ? AtMemoryScreenState.LOADING
                    : AtMemoryScreenState.ZERO_STATE;
        }
        return AtMemoryScreenState.SUGGESTIONS;
    }

    private void applyScreenState(
            AtMemoryScreenState screenState, List<AutofillSuggestion> suggestions) {
        mHomeModel.set(HomeProperties.IS_LOADING, screenState.isLoading);
        mHomeModel.set(
                HomeProperties.SHOW_SUGGESTIONS_BACKGROUND, screenState.showSuggestionsBackground);

        ModelList sheetItems = mHomeModel.get(HomeProperties.SHEET_ITEMS);
        if (screenState.showZeroState) {
            applyZeroState(sheetItems);
        }
        if (screenState.showSearchAffordance) {
            applySearchAffordance(suggestions.get(0), sheetItems);
        }
        if (screenState.showAtMemorySuggestions) {
            applySuggestions(suggestions, sheetItems);
        }
        if (screenState == AtMemoryScreenState.HIDDEN) {
            mModel.set(VISIBLE, false);
            mModel.set(CURRENT_SCREEN, ScreenId.HOME_SCREEN);
            mFlyoutModel.set(FlyoutProperties.TITLE, "");
            mFlyoutModel.set(FlyoutProperties.SUGGESTIONS, List.of());
            sheetItems.clear();
            return;
        }
    }

    private void applyZeroState(ModelList sheetItems) {
        if (sheetItems.size() == 1
                && sheetItems.get(0).type == HomeProperties.ItemType.ZERO_STATE) {
            return;
        }
        sheetItems.clear();
        sheetItems.add(new ListItem(HomeProperties.ItemType.ZERO_STATE, new PropertyModel()));
    }

    private void applySearchAffordance(AutofillSuggestion affordance, ModelList sheetItems) {
        if (!sheetItems.isEmpty()
                && sheetItems.get(0).type == HomeProperties.ItemType.SEARCH_TILE) {
            sheetItems.get(0).model.set(SearchItemProperties.TILE_TITLE, affordance.getLabel());
            if (sheetItems.size() > 1) {
                sheetItems.removeRange(1, sheetItems.size() - 1);
            }
            return;
        }
        sheetItems.clear();
        PropertyModel itemModel =
                new PropertyModel.Builder(SearchItemProperties.ALL_KEYS)
                        .with(SearchItemProperties.TILE_ICON, affordance.getIconId())
                        .with(SearchItemProperties.TILE_TITLE, affordance.getLabel())
                        .with(SearchItemProperties.TILE_DETAILS, affordance.getSublabel())
                        .with(SearchItemProperties.ON_TILE_CLICKED, this::onSearchTileClicked)
                        .build();
        sheetItems.add(new ListItem(HomeProperties.ItemType.SEARCH_TILE, itemModel));
    }

    private void applySuggestions(List<AutofillSuggestion> suggestions, ModelList sheetItems) {
        sheetItems.clear();
        for (int i = 0; i < suggestions.size(); i++) {
            AutofillSuggestion suggestion = suggestions.get(i);
            int position = i;
            PropertyModel itemModel =
                    new PropertyModel.Builder(SuggestionItemProperties.ALL_KEYS)
                            .with(SuggestionItemProperties.ICON, suggestion.getIconId())
                            .with(SuggestionItemProperties.TITLE, suggestion.getLabel())
                            .with(SuggestionItemProperties.DETAILS, suggestion.getSublabel())
                            .with(
                                    SuggestionItemProperties.ON_SUGGESTION_CLICKED,
                                    () -> onSuggestionClicked(position))
                            .with(
                                    SuggestionItemProperties.ON_FLYOUT_CLICKED,
                                    () -> onFlyoutClicked(suggestion, position))
                            .build();
            sheetItems.add(new ListItem(HomeProperties.ItemType.SUGGESTION, itemModel));
        }
    }

    private void onSuggestionClicked(int position) {
        mDelegate.onSuggestionClicked(position);
    }

    private void onFlyoutClicked(AutofillSuggestion suggestion, int position) {
        mFlyoutModel.set(FlyoutProperties.TITLE, suggestion.getLabel());
        mFlyoutModel.set(FlyoutProperties.SUGGESTIONS, suggestion.getChildren());
        mFlyoutModel.set(
                FlyoutProperties.ON_SUGGESTION_CLICKED,
                childPosition -> onFlyoutSuggestionClicked(position, childPosition));

        mModel.set(CURRENT_SCREEN, ScreenId.FLYOUT_SCREEN);
        mDelegate.onChildSuggestionsShown(position);
    }

    private void onFlyoutBackClicked() {
        mModel.set(CURRENT_SCREEN, ScreenId.HOME_SCREEN);
    }

    private void onFlyoutManageClicked() {
        // TODO(crbug.com/505255929): Implement manage clicked handler
    }

    private void onFlyoutSuggestionClicked(int parentPosition, int childPosition) {
        mDelegate.onChildSuggestionClicked(parentPosition, childPosition);
    }

    private void onSearchTileClicked() {
        ModelList sheetItems = mHomeModel.get(HomeProperties.SHEET_ITEMS);
        if (sheetItems.isEmpty()) return;

        ListItem item = sheetItems.get(0);
        if (item.type != HomeProperties.ItemType.SEARCH_TILE) return;

        String query = item.model.get(SearchItemProperties.TILE_TITLE);
        if (query == null) return;

        mSearchDelegate.hideKeyboardAndClearFocus();
        onQuerySubmitted(query);
    }

    @Override
    public void onQuerySubmitted(String query) {
        mHomeModel.set(HomeProperties.IS_LOADING, true);
        mDelegate.onQuerySubmitted(query);
    }

    @Override
    public void onQueryTextChanged(String query) {
        mDelegate.onQueryTextChanged(query);
    }

    @Override
    public void onSearchFocus(boolean hasFocus) {
        mDelegate.onSearchFocus(hasFocus);
    }

    private boolean isSearchAffordance(List<AutofillSuggestion> suggestions) {
        return suggestions.size() == 1
                && suggestions.get(0).getSuggestionType()
                        == SuggestionType.AT_MEMORY_SEARCH_AFFORDANCE;
    }

    private PropertyModel createModel() {
        return new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                .with(VISIBLE, false)
                .with(CURRENT_SCREEN, ScreenId.HOME_SCREEN)
                .build();
    }

    private PropertyModel createHomeModel() {
        boolean shouldShowNotice =
                PersonalContextFirstRunService.shouldShowAtMemoryNotice(mProfile);
        return new PropertyModel.Builder(HomeProperties.ALL_KEYS)
                .with(HomeProperties.IS_LOADING, false)
                .with(HomeProperties.SHOW_SUGGESTIONS_BACKGROUND, false)
                .with(HomeProperties.SHEET_ITEMS, new ModelList())
                .with(HomeProperties.SEARCH_BAR_DELEGATE, this)
                .with(HomeProperties.IS_NOTICE_VISIBLE, shouldShowNotice)
                .with(HomeProperties.NOTICE_OK_CLICK_LISTENER, this::onNoticeAcknowledged)
                .with(HomeProperties.NOTICE_SETTINGS_CLICK_LISTENER, this::onNoticeSettingsClicked)
                .build();
    }

    private PropertyModel createFlyoutModel() {
        return new PropertyModel.Builder(FlyoutProperties.ALL_KEYS)
                .with(FlyoutProperties.TITLE, "")
                .with(FlyoutProperties.SUGGESTIONS, List.of())
                .with(FlyoutProperties.ON_BACK_CLICKED, this::onFlyoutBackClicked)
                .with(FlyoutProperties.ON_MANAGE_CLICKED, this::onFlyoutManageClicked)
                .with(FlyoutProperties.ON_SUGGESTION_CLICKED, childPos -> {})
                .build();
    }
}
