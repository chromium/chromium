// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Properties defined here reflect the visible state of the AtMemoryBottomSheet. */
@NullMarked
class AtMemoryBottomSheetProperties {
    // Indicates whether the bottom sheet dialog should be visible.
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    // Reflects which screen is currently displayed (main vs flyout).
    static final WritableIntPropertyKey CURRENT_SCREEN = new WritableIntPropertyKey();

    static final PropertyKey[] ALL_KEYS = {VISIBLE, CURRENT_SCREEN};

    /** Identifies different screens that can be dynamically displayed by the bottom sheet. */
    @IntDef({ScreenId.HOME_SCREEN, ScreenId.FLYOUT_SCREEN})
    @Retention(RetentionPolicy.SOURCE)
    @interface ScreenId {
        /** The home screen, where the search bar and suggestions are displayed. */
        int HOME_SCREEN = 0;

        /** The flyout screen, where the flyout suggestions are displayed. */
        int FLYOUT_SCREEN = 1;
    }

    // Property keys for the suggestion list items.
    static class HomeProperties {
        // Indicates whether the bottom sheet dialog should display a loading state.
        static final WritableBooleanPropertyKey IS_LOADING = new WritableBooleanPropertyKey();

        static final ReadableObjectPropertyKey<AtMemorySearchBarView.Delegate> SEARCH_BAR_DELEGATE =
                new ReadableObjectPropertyKey<>();

        // Items to be displayed in the bottom sheet (only for home screen).
        static final ReadableObjectPropertyKey<ModelList> SHEET_ITEMS =
                new ReadableObjectPropertyKey<>();

        static final PropertyKey[] ALL_KEYS = {IS_LOADING, SEARCH_BAR_DELEGATE, SHEET_ITEMS};

        @IntDef({
            ItemType.SUGGESTION,
            ItemType.SUGGESTION_WITH_NO_BACKGROUND,
            ItemType.ZERO_STATE,
            ItemType.NOTICE,
            ItemType.TEXT_WITH_CLICKABLE_LINK
        })
        @Retention(RetentionPolicy.SOURCE)
        @interface ItemType {
            /** A section containing suggestions. */
            int SUGGESTION = 0;

            /** A section containing suggestions with no background card. */
            int SUGGESTION_WITH_NO_BACKGROUND = 1;

            /** A section containing no results. */
            int ZERO_STATE = 2;

            /** A section containing onboarding notice. */
            int NOTICE = 3;

            /** A section containing text with a clickable link. */
            int TEXT_WITH_CLICKABLE_LINK = 4;
        }

        /** Delegate to request search UI actions (e.g. hiding keyboard or clearing focus). */
        public interface SearchDelegate {
            /** Hides the keyboard and clears focus from the search area. */
            void hideKeyboardAndClearFocus();
        }

        private HomeProperties() {}
    }

    /** Properties for the flyout screen within the bottom sheet. */
    static class FlyoutProperties {
        // Title to be displayed on the flyout screen.
        static final WritableObjectPropertyKey<@Nullable String> TITLE =
                new WritableObjectPropertyKey<>();
        // List of autofill suggestions to be shown in the flyout screen.
        static final WritableObjectPropertyKey<List<AutofillSuggestion>> SUGGESTIONS =
                new WritableObjectPropertyKey<>();
        // Invoked when the back button is clicked in the flyout screen.
        static final ReadableObjectPropertyKey<Runnable> ON_BACK_CLICKED =
                new ReadableObjectPropertyKey<>();
        // Invoked when an autofill suggestion is clicked in the flyout screen.
        static final WritableObjectPropertyKey<Callback<Integer>> ON_SUGGESTION_CLICKED =
                new WritableObjectPropertyKey<>();

        static final PropertyKey[] ALL_KEYS = {
            TITLE, SUGGESTIONS, ON_BACK_CLICKED, ON_SUGGESTION_CLICKED
        };

        private FlyoutProperties() {}
    }

    /** Properties for the notice item displayed within the home screen. */
    static class NoticeItemProperties {
        // Invoked when the user acknowledges the onboarding notice.
        static final ReadableObjectPropertyKey<Runnable> ON_OK_CLICKED =
                new ReadableObjectPropertyKey<>();
        // Invoked when the user clicks on the "Settings" link in the onboarding notice.
        static final ReadableObjectPropertyKey<Runnable> ON_SETTINGS_CLICKED =
                new ReadableObjectPropertyKey<>();

        static final PropertyKey[] ALL_KEYS = {ON_OK_CLICKED, ON_SETTINGS_CLICKED};

        private NoticeItemProperties() {}
    }

    /** Properties for the suggestion items displayed within the home screen. */
    static class SuggestionItemProperties {
        // Icon to be displayed in the suggestion item.
        static final ReadableIntPropertyKey ICON = new ReadableIntPropertyKey();
        // Title to be displayed in the suggestion item.
        static final WritableObjectPropertyKey<@Nullable String> TITLE =
                new WritableObjectPropertyKey<>();
        // Details to be displayed in the suggestion item.
        static final ReadableObjectPropertyKey<String> DETAILS = new ReadableObjectPropertyKey<>();
        // Invoked when the suggestion item is clicked.
        static final ReadableObjectPropertyKey<Runnable> ON_SUGGESTION_CLICKED =
                new ReadableObjectPropertyKey<>();
        // Invoked when the flyout button is clicked on the suggestion item.
        static final ReadableObjectPropertyKey<Runnable> ON_FLYOUT_CLICKED =
                new ReadableObjectPropertyKey<>();
        // Indicates whether the flyout arrow and divider should be visible.
        static final WritableBooleanPropertyKey IS_FLYOUT_VISIBLE =
                new WritableBooleanPropertyKey();
        // Resource ID for trailing icon in the suggestion item (0 if hidden).
        static final ReadableIntPropertyKey TRAILING_ICON_ID = new ReadableIntPropertyKey();
        // Indicates whether the suggestion item should be deactivated.
        static final WritableBooleanPropertyKey APPLY_DEACTIVATED_STYLE =
                new WritableBooleanPropertyKey();

        static final PropertyKey[] ALL_KEYS = {
            ICON,
            TRAILING_ICON_ID,
            TITLE,
            DETAILS,
            ON_SUGGESTION_CLICKED,
            ON_FLYOUT_CLICKED,
            IS_FLYOUT_VISIBLE,
            APPLY_DEACTIVATED_STYLE
        };

        private SuggestionItemProperties() {}
    }

    /** Properties for text items with clickable link displayed within the home screen. */
    static class TextWithClickableLinkProperties {
        // Text to be displayed with clickable link formatting.
        static final ReadableObjectPropertyKey<String> TEXT = new ReadableObjectPropertyKey<>();
        // Invoked when the link in the text is clicked.
        static final ReadableObjectPropertyKey<Runnable> ON_LINK_CLICKED =
                new ReadableObjectPropertyKey<>();

        static final PropertyKey[] ALL_KEYS = {TEXT, ON_LINK_CLICKED};

        private TextWithClickableLinkProperties() {}
    }

    private AtMemoryBottomSheetProperties() {}
}
