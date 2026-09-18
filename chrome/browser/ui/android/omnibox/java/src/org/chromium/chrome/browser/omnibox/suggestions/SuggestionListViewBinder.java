// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Handles property updates to the suggestion list component. */
@NullMarked
class SuggestionListViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<
                PropertyModel, SuggestionListViewBinder.SuggestionListViewHolder, PropertyKey> {

    /** Holds the view components needed to renderer the suggestion list. */
    public static class SuggestionListViewHolder {
        public final OmniboxSuggestionsContainer container;
        public final OmniboxSuggestionsDropdown dropdown;

        public SuggestionListViewHolder(
                OmniboxSuggestionsContainer container, OmniboxSuggestionsDropdown dropdown) {
            this.container = container;
            this.dropdown = dropdown;
        }
    }

    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    @Override
    public void bind(PropertyModel model, SuggestionListViewHolder view, PropertyKey propertyKey) {
        // The resource provider must be set before any other properties are set and binded b/c
        // some properties depend on it.
        view.dropdown.setResourceProvider(model.get(SuggestionListProperties.RESOURCE_PROVIDER));

        if (propertyKey == SuggestionListProperties.ACTIVITY_WINDOW_FOCUSED) {
            updateContainerVisibility(model, view);
        } else if (propertyKey == SuggestionListProperties.ALPHA) {
            view.dropdown.setChildAlpha(model.get(SuggestionListProperties.ALPHA));
        } else if (propertyKey == SuggestionListProperties.APPLY_MARGIN_FOR_LEFT_SIDE_BAR) {
            updateContainerMargin(model, view);
        } else if (propertyKey == SuggestionListProperties.APPLY_VERTICAL_PADDING) {
            updateVerticalPadding(model, view);
        } else if (propertyKey == SuggestionListProperties.CHILD_TRANSLATION_Y) {
            view.dropdown.translateChildrenVertical(
                    model.get(SuggestionListProperties.CHILD_TRANSLATION_Y));
        } else if (propertyKey == SuggestionListProperties.COLOR_SCHEME) {
            @BrandedColorScheme int scheme = model.get(SuggestionListProperties.COLOR_SCHEME);
            view.dropdown.setBrandedColorScheme(scheme);
            updateColorScheme(model, view);
        } else if (propertyKey == SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE) {
            if (model.get(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE)) {
                updateColorScheme(model, view);
            }
            updateContainerVisibility(model, view);
        } else if (propertyKey == SuggestionListProperties.DRAW_OVER_ANCHOR) {
            boolean drawOver = model.get(SuggestionListProperties.DRAW_OVER_ANCHOR);
            // Note: this assumes the anchor view's z hasn't been modified. If this changes, we'll
            // need to wire that z value so that we choose the correct one here.
            view.container.setTranslationZ(
                    drawOver
                            ? view.container
                                    .getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.omnibox_suggestion_list_elevation)
                            : 0.0f);
        } else if (propertyKey == SuggestionListProperties.DROPDOWN_HEIGHT_CHANGE_LISTENER) {
            view.container.setHeightChangeListener(
                    model.get(SuggestionListProperties.DROPDOWN_HEIGHT_CHANGE_LISTENER));
        } else if (propertyKey == SuggestionListProperties.DROPDOWN_SCROLL_LISTENER) {
            view.dropdown
                    .getLayoutScrollListener()
                    .setSuggestionDropdownScrollListener(
                            model.get(SuggestionListProperties.DROPDOWN_SCROLL_LISTENER));
        } else if (propertyKey == SuggestionListProperties.DROPDOWN_SCROLL_OFFSET_LISTENER) {
            view.dropdown
                    .getLayoutScrollListener()
                    .setScrollOffsetListener(
                            model.get(SuggestionListProperties.DROPDOWN_SCROLL_OFFSET_LISTENER));
        } else if (propertyKey == SuggestionListProperties.DROPDOWN_SCROLL_TO_TOP_LISTENER) {
            view.dropdown
                    .getLayoutScrollListener()
                    .setSuggestionDropdownOverscrolledToTopListener(
                            model.get(SuggestionListProperties.DROPDOWN_SCROLL_TO_TOP_LISTENER));
        } else if (propertyKey == SuggestionListProperties.EMBEDDER) {
            view.container.setEmbedder(model.get(SuggestionListProperties.EMBEDDER));
        } else if (propertyKey == SuggestionListProperties.GESTURE_OBSERVER) {
            view.dropdown.setGestureObserver(model.get(SuggestionListProperties.GESTURE_OBSERVER));
        } else if (propertyKey == SuggestionListProperties.IS_LARGE_SCREEN) {
            updateRoundingAndClipping(model, view);
        } else if (propertyKey == SuggestionListProperties.LEFT_SIDE_BAR_MARGIN_PX) {
            updateContainerMargin(model, view);
        } else if (propertyKey == SuggestionListProperties.LIST_IS_FINAL) {
            if (model.get(SuggestionListProperties.LIST_IS_FINAL)) {
                view.dropdown.emitWindowContentChangedAnnouncement();
            }
        } else if (propertyKey == SuggestionListProperties.NAVIGATION_LISTENER) {
            view.dropdown.setNavigationListener(
                    model.get(SuggestionListProperties.NAVIGATION_LISTENER));
        } else if (propertyKey == SuggestionListProperties.OMNIBOX_SESSION_ACTIVE) {
            updateContainerVisibility(model, view);
            view.container.onOmniboxSessionStateChange(
                    model.get(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE));
        } else if (propertyKey == SuggestionListProperties.RESET_SELECTION) {
            view.dropdown.resetSelection();
        } else if (propertyKey == SuggestionListProperties.RESOURCE_PROVIDER) {
            view.dropdown.setResourceProvider(
                    model.get(SuggestionListProperties.RESOURCE_PROVIDER));
        } else if (propertyKey == SuggestionListProperties.ROUND_TOP_CORNERS) {
            updateRoundingAndClipping(model, view);
        } else if (propertyKey == SuggestionListProperties.SELECTION_MODE) {
            view.dropdown.setSelectionMode(model.get(SuggestionListProperties.SELECTION_MODE));
        } else if (propertyKey == SuggestionListProperties.SUGGESTION_MODELS) {
            ModelList listItems = model.get(SuggestionListProperties.SUGGESTION_MODELS);
            listItems.addObserver(
                    new ListObservable.ListObserver<>() {
                        @Override
                        public void onItemRangeInserted(
                                ListObservable source, int index, int count) {
                            view.dropdown.resetSelection();
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
        }
    }

    private void updateRoundingAndClipping(PropertyModel model, SuggestionListViewHolder holder) {
        updateColorScheme(model, holder);
        boolean isLargeScreen = model.get(SuggestionListProperties.IS_LARGE_SCREEN);
        boolean roundTopCorners = model.get(SuggestionListProperties.ROUND_TOP_CORNERS);
        holder.container.setShouldRoundTopCorners(roundTopCorners);
        holder.container.setShouldClipToOutline(isLargeScreen || roundTopCorners);
    }

    private void updateColorScheme(PropertyModel model, SuggestionListViewHolder holder) {
        @FuseboxLayoutMode int layoutMode = model.get(SuggestionListProperties.FUSEBOX_LAYOUT_MODE);
        @ColorInt
        int backgroundColor =
                getResourceProvider(model)
                        .getSuggestionBackgroundColor(layoutMode, /* isDropdownContainer= */ true);

        holder.dropdown.setBackgroundColor(backgroundColor);

        if (model.get(SuggestionListProperties.IS_LARGE_SCREEN)
                && layoutMode != FuseboxLayoutMode.SUGGESTIONS_POPOVER) {
            holder.container.setBackgroundColor(Color.TRANSPARENT);
        } else {
            holder.container.setBackgroundColor(backgroundColor);
        }
    }

    private static void updateVerticalPadding(PropertyModel model, SuggestionListViewHolder holder) {
        boolean applyVerticalPadding = model.get(SuggestionListProperties.APPLY_VERTICAL_PADDING);
        @Px
        int topPadding =
                applyVerticalPadding ? getResourceProvider(model).getDropdownTopPadding() : 0;
        @Px
        int bottomPadding =
                applyVerticalPadding ? getResourceProvider(model).getDropdownBottomPadding() : 0;
        holder.dropdown.setVerticalPadding(topPadding, bottomPadding);
    }

    private static void updateContainerVisibility(
            PropertyModel model, SuggestionListViewHolder holder) {
        ModelList listItems = model.get(SuggestionListProperties.SUGGESTION_MODELS);
        boolean shouldListBeVisible =
                model.get(SuggestionListProperties.ACTIVITY_WINDOW_FOCUSED)
                        && model.get(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE)
                        && listItems.size() > 0;
        boolean shouldContainerBeVisible =
                model.get(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE)
                        && (listItems.size() > 0
                                || model.get(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE));
        int listVisibility = shouldListBeVisible ? View.VISIBLE : View.GONE;
        int containerVisibility = shouldContainerBeVisible ? View.VISIBLE : View.GONE;
        holder.container.setVisibility(containerVisibility);
        holder.dropdown.setVisibility(listVisibility);
        updateContainerMargin(model, holder);
    }

    private static OmniboxResourceProvider getResourceProvider(PropertyModel model) {
        return assumeNonNull(model.get(SuggestionListProperties.RESOURCE_PROVIDER));
    }

    private static void updateContainerMargin(
            PropertyModel model, SuggestionListViewHolder holder) {
        OmniboxSuggestionsContainer container = holder.container;
        var layoutParams = (ViewGroup.MarginLayoutParams) container.getLayoutParams();
        if (layoutParams == null) {
            layoutParams =
                    new ViewGroup.MarginLayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT);
        }
        boolean applyMargin = model.get(SuggestionListProperties.APPLY_MARGIN_FOR_LEFT_SIDE_BAR);
        int marginPx = model.get(SuggestionListProperties.LEFT_SIDE_BAR_MARGIN_PX);
        int leftMargin = applyMargin ? marginPx : 0;
        if (layoutParams.leftMargin != leftMargin) {
            layoutParams.leftMargin = leftMargin;
            container.setLayoutParams(layoutParams);
        }
    }
}
