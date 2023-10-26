// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Handles property updates to the suggestion list component. */
class SuggestionListViewBinder {
    /** Holds the view components needed to renderer the suggestion list. */
    public static class SuggestionListViewHolder {
        public final ViewGroup container;
        public final OmniboxSuggestionsDropdown dropdown;

        public SuggestionListViewHolder(ViewGroup container, OmniboxSuggestionsDropdown dropdown) {
            this.container = container;
            this.dropdown = dropdown;
        }
    }

    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(
            PropertyModel model, SuggestionListViewHolder view, PropertyKey propertyKey) {
        if (SuggestionListProperties.VISIBLE.equals(propertyKey)) {
            boolean visible = model.get(SuggestionListProperties.VISIBLE);
            // Actual View showing the dropdown.
            View dropdownView = view.dropdown.getViewGroup();
            if (visible) {
                // Ensure the tracked keyboard state is consistent with actual keyboard state.
                // The keyboard is about to be called up.
                view.dropdown.resetKeyboardShownState();
                if (dropdownView.getParent() == null) {
                    view.container.addView(dropdownView);
                    // When showing the suggestions list for the first time, make sure to apply
                    // appropriate visibility to freshly inflated container.
                    // This is later handled by subsequent calls to updateContainerVisibility()
                    // performed whenever the suggestion model list changes.
                    updateContainerVisibility(model, view.container);
                }
            } else {
                UiUtils.removeViewFromParent(dropdownView);
            }
        } else if (SuggestionListProperties.EMBEDDER.equals(propertyKey)) {
            view.dropdown.setEmbedder(model.get(SuggestionListProperties.EMBEDDER));
        } else if (SuggestionListProperties.GESTURE_OBSERVER.equals(propertyKey)) {
            view.dropdown.setGestureObserver(model.get(SuggestionListProperties.GESTURE_OBSERVER));
        } else if (SuggestionListProperties.DROPDOWN_HEIGHT_CHANGE_LISTENER.equals(propertyKey)) {
            view.dropdown.setHeightChangeListener(
                    model.get(SuggestionListProperties.DROPDOWN_HEIGHT_CHANGE_LISTENER));
        } else if (SuggestionListProperties.DROPDOWN_SCROLL_LISTENER.equals(propertyKey)) {
            view.dropdown.setSuggestionDropdownScrollListener(
                    model.get(SuggestionListProperties.DROPDOWN_SCROLL_LISTENER));
        } else if (SuggestionListProperties.DROPDOWN_SCROLL_TO_TOP_LISTENER.equals(propertyKey)) {
            view.dropdown.setSuggestionDropdownOverscrolledToTopListener(
                    model.get(SuggestionListProperties.DROPDOWN_SCROLL_TO_TOP_LISTENER));
        } else if (SuggestionListProperties.LIST_IS_FINAL.equals(propertyKey)) {
            if (model.get(SuggestionListProperties.LIST_IS_FINAL)) {
                view.dropdown.emitWindowContentChanged();
            }
        } else if (SuggestionListProperties.SUGGESTION_MODELS.equals(propertyKey)) {
            ModelList listItems = model.get(SuggestionListProperties.SUGGESTION_MODELS);
            listItems.addObserver(
                    new ListObservable.ListObserver<Void>() {
                        @Override
                        public void onItemRangeChanged(
                                ListObservable<Void> source,
                                int index,
                                int count,
                                @Nullable Void payload) {
                            view.dropdown.resetSelection();
                        }

                        @Override
                        public void onItemRangeInserted(
                                ListObservable source, int index, int count) {
                            updateContainerVisibility(model, view.container);
                        }

                        @Override
                        public void onItemRangeRemoved(
                                ListObservable source, int index, int count) {
                            updateContainerVisibility(model, view.container);
                        }
                    });
        } else if (SuggestionListProperties.COLOR_SCHEME.equals(propertyKey)) {
            view.dropdown.refreshPopupBackground(model.get(SuggestionListProperties.COLOR_SCHEME));
        } else if (SuggestionListProperties.DRAW_OVER_ANCHOR == propertyKey) {
            boolean drawOver = model.get(SuggestionListProperties.DRAW_OVER_ANCHOR);
            // Note: this assumes the anchor view's z hasn't been modified. If this changes, we'll
            // need to wire that z value so that we choose the correct one here.
            view.container.setZ(drawOver ? 1.0f : 0.0f);
            view.dropdown.setElevation(
                    view.dropdown
                            .getResources()
                            .getDimensionPixelSize(R.dimen.omnibox_suggestion_list_elevation));
        }
    }

    private static void updateContainerVisibility(PropertyModel model, ViewGroup container) {
        ModelList listItems = model.get(SuggestionListProperties.SUGGESTION_MODELS);
        container.setVisibility(listItems.size() == 0 ? View.GONE : View.VISIBLE);
    }
}
