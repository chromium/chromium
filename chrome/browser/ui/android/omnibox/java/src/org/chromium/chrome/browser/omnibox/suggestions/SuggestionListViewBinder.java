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
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.ui.base.ViewUtils;
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

        if (SuggestionListProperties.ACTIVITY_WINDOW_FOCUSED.equals(propertyKey)) {
            updateContainerVisibility(model, view);
        } else if (SuggestionListProperties.SELECTION_MODE.equals(propertyKey)) {
            view.dropdown.setSelectionMode(model.get(SuggestionListProperties.SELECTION_MODE));
        } else if (SuggestionListProperties.ALPHA.equals(propertyKey)) {
            view.dropdown.setChildAlpha(model.get(SuggestionListProperties.ALPHA));
        } else if (SuggestionListProperties.APPLY_VERTICAL_PADDING.equals(propertyKey)) {
            updateVerticalPadding(model, view);
        } else if (SuggestionListProperties.CHILD_TRANSLATION_Y.equals(propertyKey)) {
            view.dropdown.translateChildrenVertical(
                    model.get(SuggestionListProperties.CHILD_TRANSLATION_Y));
        } else if (SuggestionListProperties.COLOR_SCHEME.equals(propertyKey)) {
            @BrandedColorScheme int scheme = model.get(SuggestionListProperties.COLOR_SCHEME);
            view.dropdown.setBrandedColorScheme(scheme);
            updateColorScheme(model, view);
        } else if (SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE.equals(propertyKey)) {
            if (model.get(SuggestionListProperties.CONTAINER_ALWAYS_VISIBLE)) {
                updateColorScheme(model, view);
            }
            updateContainerVisibility(model, view);
        } else if (SuggestionListProperties.DRAW_OVER_ANCHOR.equals(propertyKey)) {
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
        } else if (SuggestionListProperties.DROPDOWN_HEIGHT_CHANGE_LISTENER.equals(propertyKey)) {
            view.container.setHeightChangeListener(
                    model.get(SuggestionListProperties.DROPDOWN_HEIGHT_CHANGE_LISTENER));
        } else if (SuggestionListProperties.DROPDOWN_SCROLL_LISTENER.equals(propertyKey)) {
            view.dropdown
                    .getLayoutScrollListener()
                    .setSuggestionDropdownScrollListener(
                            model.get(SuggestionListProperties.DROPDOWN_SCROLL_LISTENER));
        } else if (SuggestionListProperties.DROPDOWN_SCROLL_OFFSET_LISTENER.equals(propertyKey)) {
            view.dropdown
                    .getLayoutScrollListener()
                    .setScrollOffsetListener(
                            model.get(SuggestionListProperties.DROPDOWN_SCROLL_OFFSET_LISTENER));
        } else if (SuggestionListProperties.DROPDOWN_SCROLL_TO_TOP_LISTENER.equals(propertyKey)) {
            view.dropdown
                    .getLayoutScrollListener()
                    .setSuggestionDropdownOverscrolledToTopListener(
                            model.get(SuggestionListProperties.DROPDOWN_SCROLL_TO_TOP_LISTENER));
        } else if (SuggestionListProperties.EMBEDDER.equals(propertyKey)) {
            view.container.setEmbedder(model.get(SuggestionListProperties.EMBEDDER));
        } else if (SuggestionListProperties.GESTURE_OBSERVER.equals(propertyKey)) {
            view.dropdown.setGestureObserver(model.get(SuggestionListProperties.GESTURE_OBSERVER));
        } else if (SuggestionListProperties.IS_LARGE_SCREEN.equals(propertyKey)
                || SuggestionListProperties.ROUND_TOP_CORNERS.equals(propertyKey)) {
            updateColorScheme(model, view);
            boolean isLargeScreen = model.get(SuggestionListProperties.IS_LARGE_SCREEN);
            boolean roundTopCorners = model.get(SuggestionListProperties.ROUND_TOP_CORNERS);
            view.container.setShouldRoundTopCorners(roundTopCorners);
            view.container.setShouldClipToOutline(isLargeScreen || roundTopCorners);
        } else if (SuggestionListProperties.LIST_IS_FINAL.equals(propertyKey)) {
            if (model.get(SuggestionListProperties.LIST_IS_FINAL)) {
                view.dropdown.emitWindowContentChangedAnnouncement();
            }
        } else if (SuggestionListProperties.NAVIGATION_LISTENER.equals(propertyKey)) {
            view.dropdown.setNavigationListener(
                    model.get(SuggestionListProperties.NAVIGATION_LISTENER));
        } else if (SuggestionListProperties.OMNIBOX_SESSION_ACTIVE.equals(propertyKey)) {
            updateContainerVisibility(model, view);
            view.container.onOmniboxSessionStateChange(
                    model.get(SuggestionListProperties.OMNIBOX_SESSION_ACTIVE));
        } else if (SuggestionListProperties.RESET_SELECTION.equals(propertyKey)) {
            view.dropdown.resetSelection();
        } else if (SuggestionListProperties.RESOURCE_PROVIDER.equals(propertyKey)) {
            view.dropdown.setResourceProvider(
                    model.get(SuggestionListProperties.RESOURCE_PROVIDER));
        } else if (SuggestionListProperties.SUGGESTION_MODELS.equals(propertyKey)) {
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
        } else if (SuggestionListProperties.APPLY_MARGIN_FOR_LEFT_SIDE_BAR.equals(propertyKey)) {
            updateContainerMargin(model, view);
        }
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
        // TODO(crbug.com/521986417): Consider plumbing SideUiStateProvider to get the
        // Vertical Tabs panel width. Using the constant works for now since the width is
        // fixed in MVP.
        boolean applyMargin = model.get(SuggestionListProperties.APPLY_MARGIN_FOR_LEFT_SIDE_BAR);
        int leftMargin =
                applyMargin
                        ? ViewUtils.dpToPx(
                                container.getContext(), VerticalTabUtils.SIDE_UI_CONTAINER_WIDTH_DP)
                        : 0;
        if (layoutParams.leftMargin != leftMargin) {
            layoutParams.leftMargin = leftMargin;
            container.setLayoutParams(layoutParams);
        }
    }
}
