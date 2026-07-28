// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.VISIBLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.FlyoutProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.HomeProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.NoticeItemProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.SuggestionItemProperties;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetProperties.TextWithClickableLinkProperties;
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
        } else if (propertyKey == HomeProperties.SHEET_ITEMS) {
            view.setUpSheetItems(model.get(HomeProperties.SHEET_ITEMS));
        } else {
            // Unhandled property.
            assert false : "Unhandled property: " + propertyKey;
        }
    }

    /**
     * Called whenever the notice item property model changes. It updates the given view
     * accordingly.
     *
     * @param model The model containing the notice item properties.
     * @param view The view to update.
     * @param propertyKey The property key that changed.
     */
    static void bindNoticeItemView(
            PropertyModel model, AtMemoryBottomSheetNoticeView view, PropertyKey propertyKey) {
        if (propertyKey == NoticeItemProperties.ON_OK_CLICKED) {
            view.setOkClickListener(model.get(NoticeItemProperties.ON_OK_CLICKED));
        } else if (propertyKey == NoticeItemProperties.ON_SETTINGS_CLICKED) {
            view.setSettingsClickListener(model.get(NoticeItemProperties.ON_SETTINGS_CLICKED));
        } else {
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
        } else if (propertyKey == FlyoutProperties.ON_SUGGESTION_CLICKED) {
            view.setSuggestionClickListener(model.get(FlyoutProperties.ON_SUGGESTION_CLICKED));
        } else {
            // Unhandled property.
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
        } else if (propertyKey == SuggestionItemProperties.TRAILING_ICON_ID) {
            view.setTrailingIcon(model.get(SuggestionItemProperties.TRAILING_ICON_ID));
        } else if (propertyKey == SuggestionItemProperties.APPLY_DEACTIVATED_STYLE) {
            view.applyDeactivatedStyle(model.get(SuggestionItemProperties.APPLY_DEACTIVATED_STYLE));
        } else {
            assert false : "Unhandled property: " + propertyKey;
        }
    }

    /**
     * Called whenever the text with clickable link property model changes. It updates the given
     * view accordingly.
     *
     * @param model The model containing the text with clickable link properties.
     * @param view The view to update.
     * @param propertyKey The property key that changed.
     */
    static void bindTextWithClickableLinkView(
            PropertyModel model,
            AtMemoryBottomSheetTextWithClickableLinkView view,
            PropertyKey propertyKey) {
        if (propertyKey == TextWithClickableLinkProperties.TEXT
                || propertyKey == TextWithClickableLinkProperties.ON_LINK_CLICKED) {
            view.setText(
                    model.get(TextWithClickableLinkProperties.TEXT),
                    model.get(TextWithClickableLinkProperties.ON_LINK_CLICKED));
        } else {
            assert false : "Unhandled property: " + propertyKey;
        }
    }
}
