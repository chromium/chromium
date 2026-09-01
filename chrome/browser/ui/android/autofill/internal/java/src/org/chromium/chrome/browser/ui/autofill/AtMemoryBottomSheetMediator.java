// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;

import android.content.Context;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.settings.PersonalContextSettingsLauncher;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsReferrer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.FlyoutProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.HomeProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.IllustrationCardItemProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.NoticeItemProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.ScreenId;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.TextWithClickableLinkProperties;
import org.chromium.chrome.browser.ui.autofill.internal.R;
import org.chromium.components.autofill.AtMemoryPayload;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.PopupNoticeInteractions;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Contains the business logic for the AtMemoryBottomSheet. */
@NullMarked
class AtMemoryBottomSheetMediator implements AtMemorySearchBarView.Delegate {
    static final String NOTICE_INTERACTIONS_HISTOGRAM =
            "PersonalContext.AtMemory.NoticeInteractions";

    // LINT.IfChange(FindAndFillWithGeminiSettings)
    @VisibleForTesting
    static final String FIND_AND_FILL_WITH_GEMINI_SETTINGS =
            "autofill.personal_context.find_and_fill_with_gemini_settings";

    // LINT.ThenChange(//components/optimization_guide/core/feature_registry/feature_registration.cc:FindAndFillWithGeminiSettings)

    // LINT.IfChange(AllowLogging)
    private static final int ALLOW_LOGGING = 0;

    // LINT.ThenChange(//components/optimization_guide/core/model_execution/model_execution_prefs.h:ModelExecutionEnterprisePolicyValue)

    private final Context mContext;
    private final PropertyModel mModel;
    private final PropertyModel mHomeModel;
    private final PropertyModel mFlyoutModel;
    private final AtMemoryBottomSheetCoordinator.Delegate mDelegate;
    private final HomeProperties.SearchDelegate mSearchDelegate;
    private final Profile mProfile;

    private boolean mWasNoticeShownRecorded;

    AtMemoryBottomSheetMediator(
            Context context,
            AtMemoryBottomSheetCoordinator.Delegate delegate,
            HomeProperties.SearchDelegate searchDelegate,
            Profile profile) {
        mContext = context;
        mDelegate = delegate;
        mSearchDelegate = searchDelegate;
        mProfile = profile;

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
    }

    void onDismissed() {
        applyScreenState(AtMemoryScreenState.HIDDEN, List.of());

        mDelegate.onDismissed();
    }

    void onSheetOpened() {
        mModel.set(VISIBLE, true);
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
        mModel.set(CURRENT_SCREEN, ScreenId.HOME_SCREEN);
        mHomeModel.set(HomeProperties.IS_LOADING, screenState.isLoading);

        ModelList sheetItems = mHomeModel.get(HomeProperties.SHEET_ITEMS);
        sheetItems.clear();

        boolean isNoticeVisible = hasNotice(suggestions);
        if (screenState.showZeroState) {
            sheetItems.add(
                    new ListItem(
                            HomeProperties.ItemType.ILLUSTRATION_CARD, createZeroStateModel()));
        }
        if (screenState.showAtMemorySuggestions) {
            for (int i = 0; i < suggestions.size(); i++) {
                if (shouldShowSuggestion(suggestions.get(i), isNoticeVisible)) {
                    sheetItems.add(createListItemForSuggestion(suggestions.get(i), i));
                }
            }
        }
        if (screenState == AtMemoryScreenState.HIDDEN) {
            mModel.set(VISIBLE, false);
            mFlyoutModel.set(FlyoutProperties.TITLE, "");
            mFlyoutModel.set(FlyoutProperties.SUGGESTIONS, List.of());
            sheetItems.clear();
        }
    }

    private static boolean shouldShowSuggestion(
            AutofillSuggestion suggestion, boolean isNoticeVisible) {
        if (suggestion.getSuggestionType() == SuggestionType.SEPARATOR) {
            return false;
        }
        // Do not show the fetching illustration card if the notice is visible to avoid displaying
        // multiple card banners simultaneously.
        if (suggestion.getSuggestionType() == SuggestionType.AT_MEMORY_FETCHING
                && isNoticeVisible) {
            return false;
        }
        return true;
    }

    private static boolean hasNotice(List<AutofillSuggestion> suggestions) {
        for (AutofillSuggestion suggestion : suggestions) {
            if (suggestion.getSuggestionType() == SuggestionType.PERSONAL_CONTEXT_NOTICE) {
                return true;
            }
        }
        return false;
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
        if (suggestion.getSuggestionType() == SuggestionType.AT_MEMORY_FETCHING) {
            return new ListItem(
                    HomeProperties.ItemType.ILLUSTRATION_CARD,
                    createIllustrationCardModel(suggestion));
        }
        return new ListItem(
                HomeProperties.ItemType.SUGGESTION, createSuggestionModel(suggestion, position));
    }

    private void recordNoticeShown() {
        if (!mWasNoticeShownRecorded) {
            RecordHistogram.recordEnumeratedHistogram(
                    NOTICE_INTERACTIONS_HISTOGRAM,
                    PopupNoticeInteractions.SHOWN,
                    PopupNoticeInteractions.MAX_VALUE + 1);
            mWasNoticeShownRecorded = true;
        }
    }

    private void onNoticeAcknowledged(int position) {
        RecordHistogram.recordEnumeratedHistogram(
                NOTICE_INTERACTIONS_HISTOGRAM,
                PopupNoticeInteractions.ACKNOWLEDGED,
                PopupNoticeInteractions.MAX_VALUE + 1);
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
        AtMemoryPayload payload = suggestion.getAtMemoryPayload();
        mFlyoutModel.set(FlyoutProperties.TITLE, payload != null ? payload.getTypeName() : null);
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

    private PropertyModel createIllustrationCardModel(AutofillSuggestion suggestion) {
        return new PropertyModel.Builder(IllustrationCardItemProperties.ALL_KEYS)
                .with(IllustrationCardItemProperties.TITLE, suggestion.getLabel())
                .with(IllustrationCardItemProperties.SUBTITLE, suggestion.getSublabel())
                .build();
    }

    private PropertyModel createZeroStateModel() {
        return new PropertyModel.Builder(IllustrationCardItemProperties.ALL_KEYS)
                .with(
                        IllustrationCardItemProperties.TITLE,
                        mContext.getString(R.string.autofill_at_memory_zero_state_title))
                .with(
                        IllustrationCardItemProperties.SUBTITLE,
                        mContext.getString(R.string.autofill_at_memory_zero_state_subtitle))
                .build();
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

    private boolean isLoggingAllowedByPolicy() {
        PrefService prefService = UserPrefs.get(mProfile);
        if (prefService == null) return true;
        return prefService.getInteger(FIND_AND_FILL_WITH_GEMINI_SETTINGS) == ALLOW_LOGGING;
    }

    private PropertyModel createNoticeModel(int position) {
        return new PropertyModel.Builder(NoticeItemProperties.ALL_KEYS)
                .with(NoticeItemProperties.IS_LOGGING_ALLOWED, isLoggingAllowedByPolicy())
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
                // Loading suggestions should be deactivated as well.
                // TODO(crbug.com/536814322) - Apply deactivate style to all unacceptable
                // suggestions?
                .with(
                        SuggestionItemProperties.APPLY_DEACTIVATED_STYLE,
                        suggestion.applyDeactivatedStyle() || suggestion.isLoading())
                .with(SuggestionItemProperties.IS_LOADING, suggestion.isLoading())
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
            case SuggestionType.AT_MEMORY_OPEN_GEMINI:
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
