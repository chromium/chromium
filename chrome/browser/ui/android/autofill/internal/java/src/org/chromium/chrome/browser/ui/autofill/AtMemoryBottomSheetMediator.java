// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;

import android.content.Context;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.settings.PersonalContextSettingsLauncher;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsReferrer;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.FlyoutProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.HomeProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.NoticeItemProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ScreenId;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.TextWithClickableLinkProperties;
import org.chromium.chrome.browser.ui.autofill.internal.R;
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
    private final PropertyModel mModel;
    private final PropertyModel mHomeModel;
    private final PropertyModel mFlyoutModel;
    private final AtMemoryBottomSheetCoordinator.Delegate mDelegate;
    private final HomeProperties.SearchDelegate mSearchDelegate;

    private boolean mWasNoticeShownRecorded;

    AtMemoryBottomSheetMediator(
            Context context,
            AtMemoryBottomSheetCoordinator.Delegate delegate,
            HomeProperties.SearchDelegate searchDelegate) {
        mContext = context;
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
    }

    void onDismissed() {
        applyScreenState(AtMemoryScreenState.HIDDEN, List.of());

        mDelegate.onDismissed();
    }

    private AtMemoryScreenState getScreenState(List<AutofillSuggestion> suggestions) {
        if (mDelegate.isSearching()) {
            return AtMemoryScreenState.LOADING;
        }
        if (suggestions.isEmpty()) {
            return AtMemoryScreenState.ZERO_STATE;
        }
        return AtMemoryScreenState.SUGGESTIONS;
    }

    private void applyScreenState(
            AtMemoryScreenState screenState, List<AutofillSuggestion> suggestions) {
        mHomeModel.set(HomeProperties.IS_LOADING, screenState.isLoading);

        ModelList sheetItems = mHomeModel.get(HomeProperties.SHEET_ITEMS);
        sheetItems.clear();

        if (screenState.showZeroState) {
            sheetItems.add(new ListItem(HomeProperties.ItemType.ZERO_STATE, new PropertyModel()));
        }
        if (screenState.showAtMemorySuggestions) {
            for (int i = 0; i < suggestions.size(); i++) {
                if (suggestions.get(i).getSuggestionType() != SuggestionType.SEPARATOR) {
                    sheetItems.add(createListItemForSuggestion(suggestions.get(i), i));
                }
            }
        }
        if (screenState == AtMemoryScreenState.HIDDEN) {
            mModel.set(VISIBLE, false);
            mModel.set(CURRENT_SCREEN, ScreenId.HOME_SCREEN);
            mFlyoutModel.set(FlyoutProperties.TITLE, "");
            mFlyoutModel.set(FlyoutProperties.SUGGESTIONS, List.of());
            sheetItems.clear();
        }
    }

    private ListItem createListItemForSuggestion(AutofillSuggestion suggestion, int position) {
        if (suggestion.getSuggestionType() == SuggestionType.PERSONAL_CONTEXT_NOTICE) {
            recordNoticeShown();
            return new ListItem(HomeProperties.ItemType.NOTICE, createNoticeModel(position));
        }
        if (suggestion.getSuggestionType() == SuggestionType.AT_MEMORY_SEARCH_AFFORDANCE) {
            return new ListItem(
                    HomeProperties.ItemType.SUGGESTION_WITH_NO_BACKGROUND,
                    createSuggestionModel(suggestion, position));
        }
        if (suggestion.getSuggestionType() == SuggestionType.AT_MEMORY_AI_DISCLOSURE) {
            return new ListItem(
                    HomeProperties.ItemType.TEXT_WITH_CLICKABLE_LINK, createAiDisclosureModel());
        }
        return new ListItem(
                HomeProperties.ItemType.SUGGESTION, createSuggestionModel(suggestion, position));
    }

    private void recordNoticeShown() {
        if (!mWasNoticeShownRecorded) {
            RecordHistogram.recordEnumeratedHistogram(
                    NOTICE_INTERACTIONS_HISTOGRAM,
                    NoticeInteraction.SHOWN,
                    NoticeInteraction.COUNT);
            mWasNoticeShownRecorded = true;
        }
    }

    private void onNoticeAcknowledged(int position) {
        RecordHistogram.recordEnumeratedHistogram(
                NOTICE_INTERACTIONS_HISTOGRAM,
                NoticeInteraction.ACKNOWLEDGED,
                NoticeInteraction.COUNT);
        mDelegate.onSuggestionDismissed(position);
    }

    private void onNoticeSettingsClicked() {
        RecordUserAction.record("PersonalContext.AtMemory.Notice.SettingsLinkClick");
        PersonalContextSettingsLauncher.showPersonalContextSettings(
                mContext, AutofillOptionsReferrer.PERSONAL_CONTEXT_ATMEMORY_NOTICE);
    }

    private void onSuggestionClicked(AutofillSuggestion suggestion, int position) {
        if (!suggestion.isAcceptable()) {
            return;
        }

        if (suggestion.getSuggestionType() == SuggestionType.AT_MEMORY_SEARCH_AFFORDANCE) {
            mSearchDelegate.hideKeyboardAndClearFocus();
            String query = suggestion.getLabel();
            if (query != null) {
                onQuerySubmitted(query);
            }
            return;
        }
        mDelegate.onSuggestionClicked(position);
    }

    private void onFlyoutClicked(AutofillSuggestion suggestion, int position) {
        // Assumes the secondary label contains the data type name.
        mFlyoutModel.set(FlyoutProperties.TITLE, suggestion.getSecondaryLabel());
        mFlyoutModel.set(FlyoutProperties.SUGGESTIONS, suggestion.getChildren());
        mFlyoutModel.set(
                FlyoutProperties.ON_SUGGESTION_CLICKED,
                childPosition -> onFlyoutSuggestionClicked(position, childPosition));

        mModel.set(CURRENT_SCREEN, ScreenId.FLYOUT_SCREEN);
        mDelegate.onChildSuggestionsShown(position);
        mDelegate.requestExpandSheet(/* expandInFullHeight= */ false);
    }

    private void onFlyoutBackClicked() {
        mModel.set(CURRENT_SCREEN, ScreenId.HOME_SCREEN);
        mDelegate.requestExpandSheet(/* expandInFullHeight= */ false);
    }

    private void onFlyoutSuggestionClicked(int parentPosition, int childPosition) {
        mDelegate.onChildSuggestionClicked(parentPosition, childPosition);
    }

    @Override
    public void onQuerySubmitted(String query) {
        mHomeModel.set(HomeProperties.IS_LOADING, true);
        mDelegate.onQuerySubmitted(query);
        mDelegate.requestExpandSheet(/* expandInFullHeight= */ true);
    }

    @Override
    public void onQueryTextChanged(String query) {
        mDelegate.onQueryTextChanged(query);
    }

    @Override
    public void onSearchFocus(boolean hasFocus) {
        if (hasFocus) {
            mDelegate.requestExpandSheet(/* expandInFullHeight= */ true);
        }
    }

    private PropertyModel createAiDisclosureModel() {
        String text = mContext.getString(R.string.at_memory_ai_disclosure);
        return new PropertyModel.Builder(TextWithClickableLinkProperties.ALL_KEYS)
                .with(TextWithClickableLinkProperties.TEXT, text)
                .with(
                        TextWithClickableLinkProperties.ON_LINK_CLICKED,
                        this::onNoticeSettingsClicked)
                .build();
    }

    private PropertyModel createNoticeModel(int position) {
        return new PropertyModel.Builder(NoticeItemProperties.ALL_KEYS)
                .with(NoticeItemProperties.ON_OK_CLICKED, () -> onNoticeAcknowledged(position))
                .with(NoticeItemProperties.ON_SETTINGS_CLICKED, this::onNoticeSettingsClicked)
                .build();
    }

    private PropertyModel createSuggestionModel(AutofillSuggestion suggestion, int position) {
        return new PropertyModel.Builder(SuggestionItemProperties.ALL_KEYS)
                .with(SuggestionItemProperties.ICON, suggestion.getIconId())
                .with(SuggestionItemProperties.TITLE, suggestion.getLabel())
                .with(SuggestionItemProperties.DETAILS, suggestion.getSublabel())
                .with(
                        SuggestionItemProperties.IS_FLYOUT_VISIBLE,
                        !suggestion.getChildren().isEmpty())
                .with(
                        SuggestionItemProperties.TRAILING_ICON_ID,
                        getResIdForSuggestionType(suggestion.getSuggestionType()))
                .with(
                        SuggestionItemProperties.APPLY_DEACTIVATED_STYLE,
                        suggestion.applyDeactivatedStyle())
                .with(
                        SuggestionItemProperties.ON_SUGGESTION_CLICKED,
                        () -> onSuggestionClicked(suggestion, position))
                .with(
                        SuggestionItemProperties.ON_FLYOUT_CLICKED,
                        () -> onFlyoutClicked(suggestion, position))
                .build();
    }

    private PropertyModel createModel() {
        return new PropertyModel.Builder(AtMemoryBottomSheetProperties.ALL_KEYS)
                .with(VISIBLE, false)
                .with(CURRENT_SCREEN, ScreenId.HOME_SCREEN)
                .build();
    }

    private PropertyModel createHomeModel() {
        return new PropertyModel.Builder(HomeProperties.ALL_KEYS)
                .with(HomeProperties.IS_LOADING, false)
                .with(HomeProperties.SHEET_ITEMS, new ModelList())
                .with(HomeProperties.SEARCH_BAR_DELEGATE, this)
                .build();
    }

    private PropertyModel createFlyoutModel() {
        return new PropertyModel.Builder(FlyoutProperties.ALL_KEYS)
                .with(FlyoutProperties.TITLE, "")
                .with(FlyoutProperties.SUGGESTIONS, List.of())
                .with(FlyoutProperties.ON_BACK_CLICKED, this::onFlyoutBackClicked)
                .with(FlyoutProperties.ON_SUGGESTION_CLICKED, childPos -> {})
                .build();
    }

    private @DrawableRes int getResIdForSuggestionType(int suggestionType) {
        switch (suggestionType) {
            case SuggestionType.OPEN_GEMINI:
                return R.drawable.open_in_new;
            case SuggestionType.AT_MEMORY_NO_CONNECTION:
                return R.drawable.ic_north_west_24dp;
            case SuggestionType.AT_MEMORY_SEARCH_AFFORDANCE:
                return R.drawable.ic_north_west_24dp;
            default:
                return 0;
        }
    }
}
