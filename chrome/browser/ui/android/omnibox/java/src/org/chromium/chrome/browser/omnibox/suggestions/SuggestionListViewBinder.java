// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.R;
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
        if (SuggestionListProperties.ALPHA.equals(propertyKey)) {
            view.dropdown.setChildAlpha(model.get(SuggestionListProperties.ALPHA));
        } else if (SuggestionListProperties.CHILD_TRANSLATION_Y.equals(propertyKey)) {
            view.dropdown.translateChildrenVertical(
                    model.get(SuggestionListProperties.CHILD_TRANSLATION_Y));
        } else if (SuggestionListProperties.EMBEDDER.equals(propertyKey)) {
            view.dropdown.setEmbedder(model.get(SuggestionListProperties.EMBEDDER));
        } else if (SuggestionListProperties.OMNIBOX_SESSION_ACTIVE.equals(propertyKey)) {
            updateContainerVisibility(model, view);
            view.dropdown.onOmniboxSessionStateChange(
                    model.get(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE));
        } else if (SuggestionListProperties.GESTURE_OBSERVER.equals(propertyKey)) {
            view.dropdown.setGestureObserver(model.get(SuggestionListProperties.GESTURE_OBSERVER));
        } else if (SuggestionListProperties.DROPDOWN_HEIGHT_CHANGE_LISTENER.equals(propertyKey)) {
            view.dropdown.setHeightChangeListener(
                    model.get(SuggestionListProperties.DROPDOWN_HEIGHT_CHANGE_LISTENER));
        } else if (SuggestionListProperties.DROPDOWN_SCROLL_LISTENER.equals(propertyKey)) {
            view.dropdown
                    .getLayoutScrollListener()
                    .setSuggestionDropdownScrollListener(
                            model.get(SuggestionListProperties.DROPDOWN_SCROLL_LISTENER));
        } else if (SuggestionListProperties.DROPDOWN_SCROLL_TO_TOP_LISTENER.equals(propertyKey)) {
            view.dropdown
                    .getLayoutScrollListener()
                    .setSuggestionDropdownOverscrolledToTopListener(
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
                            updateContainerVisibility(model, view);
                        }

                        @Override
                        public void onItemRangeRemoved(
                                ListObservable source, int index, int count) {
                            updateContainerVisibility(model, view);
                        }
                    });
            // When the suggestions list is installed for the first time, it may already contain
            // elements. Be sure to capture and reflect this fact appropriately.
            updateContainerVisibility(model, view);
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

    private static void updateContainerVisibility(
            PropertyModel model, SuggestionListViewHolder holder) {
        ModelList listItems = model.get(SuggestionListProperties.SUGGESTION_MODELS);
        boolean shouldBeVisible =
                model.get(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE) && listItems.size() > 0;
        int visibility = shouldBeVisible ? View.VISIBLE : View.GONE;
        holder.container.setVisibility(visibility);
        holder.dropdown.setVisibility(visibility);
    }
}
