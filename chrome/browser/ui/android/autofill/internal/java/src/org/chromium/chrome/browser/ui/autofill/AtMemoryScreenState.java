// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import org.chromium.build.annotations.NullMarked;

/**
 * Represents an immutable snapshot of the screen state for the main @memory bottom sheet UI (the
 * home screen where search queries and results are displayed). Setting the state via {@link
 * AtMemoryBottomSheetMediator#applyScreenState} is idempotent.
 *
 * <p>Examples of valid state transitions:
 *
 * <ul>
 *   <li>{@link #HIDDEN} -> {@link #ZERO_STATE}: Sheet opened with no initial search results.
 *   <li>{@link #ZERO_STATE} -> {@link #LOADING}: User inputs a query and search starts loading.
 *   <li>{@link #LOADING} -> {@link #SUGGESTIONS}: Search results arrive from the backend.
 *   <li>{@link #SUGGESTIONS} -> {@link #ZERO_STATE}: User clears the search bar query.
 *   <li>Any state -> {@link #HIDDEN}: Sheet is closed or dismissed.
 * </ul>
 */
@NullMarked
class AtMemoryScreenState {
    /**
     * Whether a loading indicator should be displayed in the search view while results are loading.
     */
    public final boolean isLoading;

    /** Whether the background visual container for suggestions should be displayed. */
    public final boolean showSuggestionsBackground;

    /**
     * Whether the zero-state illustration and prompt should be displayed. Zero state is the initial
     * informational view shown when the search input is empty or returns no results.
     */
    public final boolean showZeroState;

    /** Whether the list of autofill suggestions should be displayed. */
    public final boolean showAtMemorySuggestions;

    private AtMemoryScreenState(
            boolean isLoading,
            boolean showSuggestionsBackground,
            boolean showZeroState,
            boolean showAtMemorySuggestions) {
        this.isLoading = isLoading;
        this.showSuggestionsBackground = showSuggestionsBackground;
        this.showZeroState = showZeroState;
        this.showAtMemorySuggestions = showAtMemorySuggestions;
    }

    /** Screen state when the bottom sheet is hidden or dismissed. */
    public static final AtMemoryScreenState HIDDEN =
            new AtMemoryScreenState(
                    /* isLoading= */ false,
                    /* showSuggestionsBackground= */ false,
                    /* showZeroState= */ false,
                    /* showAtMemorySuggestions= */ false);

    /** Screen state when an asynchronous search is actively loading without prior results. */
    public static final AtMemoryScreenState LOADING =
            new AtMemoryScreenState(
                    /* isLoading= */ true,
                    /* showSuggestionsBackground= */ false,
                    /* showZeroState= */ true,
                    /* showAtMemorySuggestions= */ false);

    /** Screen state when the search input is empty or returned no results. */
    public static final AtMemoryScreenState ZERO_STATE =
            new AtMemoryScreenState(
                    /* isLoading= */ false,
                    /* showSuggestionsBackground= */ false,
                    /* showZeroState= */ true,
                    /* showAtMemorySuggestions= */ false);

    /** Screen state displaying a list of autofill suggestions. */
    public static final AtMemoryScreenState SUGGESTIONS =
            new AtMemoryScreenState(
                    /* isLoading= */ false,
                    /* showSuggestionsBackground= */ true,
                    /* showZeroState= */ false,
                    /* showAtMemorySuggestions= */ true);
}
