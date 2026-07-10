// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.FlyoutProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.HomeProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SearchItemProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds properties for the AtMemoryBottomSheet. */
@NullMarked
class AtMemoryBottomSheetViewBinder {
    /**
     * Called whenever the bottom sheet property model changes. It updates the given view
     * accordingly.
     *
     * @param model The model containing the bottom sheet properties.
     * @param view The view to update.
     * @param propertyKey The property key that changed.
     */
    static void bindAtMemoryBottomSheetView(
            PropertyModel model, AtMemoryBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey == VISIBLE) {
            if (model.get(VISIBLE)) {
                view.clearSearchText();
                view.focusSearchArea();
            }
        } else if (propertyKey == CURRENT_SCREEN) {
            view.setCurrentScreen(model.get(CURRENT_SCREEN));
        } else {
            // Unhandled property.
            assert false : "Unhandled property: " + propertyKey;
        }
    }

    /**
     * Called whenever the home view property model changes. It updates the given view accordingly.
     *
     * @param model The model containing the home view properties.
     * @param view The view to update.
     * @param propertyKey The property key that changed.
     */
    static void bindAtMemoryHomeView(
            PropertyModel model, AtMemoryHomeView view, PropertyKey propertyKey) {
        if (propertyKey == HomeProperties.SEARCH_BAR_DELEGATE) {
            view.setSearchBarDelegate(model.get(HomeProperties.SEARCH_BAR_DELEGATE));
        } else if (propertyKey == HomeProperties.IS_LOADING) {
            view.setIsLoading(model.get(HomeProperties.IS_LOADING));
        } else if (propertyKey == HomeProperties.SHOW_SUGGESTIONS_BACKGROUND) {
            view.setShowSuggestionsBackground(
                    model.get(HomeProperties.SHOW_SUGGESTIONS_BACKGROUND));
        } else if (propertyKey == HomeProperties.SHEET_ITEMS) {
            view.setUpSheetItems(model.get(HomeProperties.SHEET_ITEMS));
        } else if (propertyKey == HomeProperties.IS_NOTICE_VISIBLE) {
            view.setNoticeVisible(model.get(HomeProperties.IS_NOTICE_VISIBLE));
        } else if (propertyKey == HomeProperties.NOTICE_OK_CLICK_LISTENER) {
            view.setNoticeOkClickListener(model.get(HomeProperties.NOTICE_OK_CLICK_LISTENER));
        } else if (propertyKey == HomeProperties.NOTICE_SETTINGS_CLICK_LISTENER) {
            view.setNoticeSettingsClickListener(
                    model.get(HomeProperties.NOTICE_SETTINGS_CLICK_LISTENER));
        } else {
            // Unhandled property.
            assert false : "Unhandled property: " + propertyKey;
        }
    }

    /**
     * Called whenever the flyout property model changes. It updates the given view accordingly.
     *
     * @param model The model containing the flyout properties.
     * @param view The view to update.
     * @param propertyKey The property key that changed.
     */
    static void bindAtMemoryFlyoutView(
            PropertyModel model, AtMemoryFlyoutView view, PropertyKey propertyKey) {
        if (propertyKey == FlyoutProperties.TITLE) {
            view.setTitle(model.get(FlyoutProperties.TITLE));
        } else if (propertyKey == FlyoutProperties.SUGGESTIONS) {
            view.setSuggestions(model.get(FlyoutProperties.SUGGESTIONS));
        } else if (propertyKey == FlyoutProperties.ON_BACK_CLICKED) {
            view.setBackClickListener(model.get(FlyoutProperties.ON_BACK_CLICKED));
        } else if (propertyKey == FlyoutProperties.ON_MANAGE_CLICKED) {
            view.setManageClickListener(model.get(FlyoutProperties.ON_MANAGE_CLICKED));
        } else if (propertyKey == FlyoutProperties.ON_SUGGESTION_CLICKED) {
            view.setSuggestionClickListener(model.get(FlyoutProperties.ON_SUGGESTION_CLICKED));
        } else {
            // Unhandled property.
            assert false : "Unhandled property: " + propertyKey;
        }
    }

    /**
     * Called whenever the search item property model changes. It updates the given view
     * accordingly.
     *
     * @param model The model containing the search item properties.
     * @param view The view to update.
     * @param propertyKey The property key that changed.
     */
    static void bindSearchItemView(
            PropertyModel model, AtMemoryBottomSheetSearchTileView view, PropertyKey propertyKey) {
        if (propertyKey == SearchItemProperties.TILE_ICON) {
            view.setIcon(model.get(SearchItemProperties.TILE_ICON));
        } else if (propertyKey == SearchItemProperties.TILE_TITLE) {
            view.setTitle(model.get(SearchItemProperties.TILE_TITLE));
        } else if (propertyKey == SearchItemProperties.TILE_DETAILS) {
            view.setDetails(model.get(SearchItemProperties.TILE_DETAILS));
        } else if (propertyKey == SearchItemProperties.ON_TILE_CLICKED) {
            view.setClickListener(model.get(SearchItemProperties.ON_TILE_CLICKED));
        } else {
            assert false : "Unhandled property: " + propertyKey;
        }
    }

    /**
     * Called whenever the suggestion property model changes. It updates the given view accordingly.
     *
     * @param model The model containing the suggestion properties.
     * @param view The view to update.
     * @param propertyKey The property key that changed.
     */
    static void bindSuggestionItemView(
            PropertyModel model, AtMemoryBottomSheetSuggestionView view, PropertyKey propertyKey) {
        if (propertyKey == SuggestionItemProperties.ICON) {
            view.setIcon(model.get(SuggestionItemProperties.ICON));
        } else if (propertyKey == SuggestionItemProperties.TITLE) {
            view.setTitle(model.get(SuggestionItemProperties.TITLE));
        } else if (propertyKey == SuggestionItemProperties.DETAILS) {
            view.setDetails(model.get(SuggestionItemProperties.DETAILS));
        } else if (propertyKey == SuggestionItemProperties.ON_SUGGESTION_CLICKED) {
            view.setSuggestionClickListener(
                    model.get(SuggestionItemProperties.ON_SUGGESTION_CLICKED));
        } else if (propertyKey == SuggestionItemProperties.ON_FLYOUT_CLICKED) {
            view.setFlyoutClickListener(model.get(SuggestionItemProperties.ON_FLYOUT_CLICKED));
        } else if (propertyKey == SuggestionItemProperties.IS_FLYOUT_VISIBLE) {
            view.setFlyoutVisible(model.get(SuggestionItemProperties.IS_FLYOUT_VISIBLE));
        } else {
            assert false : "Unhandled property: " + propertyKey;
        }
    }
}
