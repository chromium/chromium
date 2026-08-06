// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.ImageView;

import androidx.appcompat.widget.SearchView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.SearchViewProvider.Observer;

/** A helper class for applying the default search behavior to search items in Chromium settings. */
@NullMarked
public class SearchUtils {
    /**
     * This interface allows to react to changed search queries when initialized with
     * {@link SearchUtils#initializeSearchView(MenuItem, String, Activity, QueryChangeListener)}.
     */
    public interface QueryChangeListener {
        /**
         * Called whenever the search query changes. This usually is immediately after a user types
         * and doesn't wait for submission of the whole query.
         * @param query Current query as entered by the user. Can be a partial query or empty.
         */
        void onQueryTextChange(String query);
    }

    /**
     * Initializes an Android default search view by setting listeners and default states of the
     * search icon, box and close icon.
     *
     * @param searchView The view that handles the search query.
     * @param initialQuery The query that the search field should be opened with.
     * @param activity Optional. If set, overflow icons in the activity's action bar will be hidden.
     * @param searchViewObserver Optional. The observer listening to the {@link SearchView}
     *     visibility.
     * @param changeListener The listener to be notified when the user changes the query.
     */
    public static void initializeSearchView(
            SearchView searchView,
            @Nullable String initialQuery,
            @Nullable Activity activity,
            @Nullable Observer searchViewObserver,
            QueryChangeListener changeListener) {
        initializeSearchView(
                /* searchItem= */ null,
                searchView,
                initialQuery,
                activity,
                searchViewObserver,
                changeListener);
    }

    /**
     * Initializes an Android default search item by setting listeners and default states of the
     * search icon, box and close icon.
     *
     * @param searchItem The existing item that can trigger the search action view.
     * @param initialQuery The query that the search field should be opened with.
     * @param activity Optional. If set, overflow icons in the activity's action bar will be hidden.
     * @param searchViewObserver The observer listening to the {@link SearchView} visibility.
     * @param changeListener The listener to be notified when the user changes the query.
     */
    public static void initializeSearchView(
            MenuItem searchItem,
            @Nullable String initialQuery,
            @Nullable Activity activity,
            @Nullable Observer searchViewObserver,
            QueryChangeListener changeListener) {
        SearchView searchView = (SearchView) searchItem.getActionView();
        assumeNonNull(searchView);
        initializeSearchView(
                searchItem, searchView, initialQuery, activity, searchViewObserver, changeListener);
    }

    private static void initializeSearchView(
            @Nullable MenuItem searchItem,
            SearchView searchView,
            @Nullable String initialQuery,
            @Nullable Activity activity,
            @Nullable Observer searchViewObserver,
            QueryChangeListener changeListener) {
        searchView.setFocusable(false);
        searchView.setImeOptions(EditorInfo.IME_FLAG_NO_FULLSCREEN);

        // Restore the search view if a query was recovered.
        if (initialQuery != null) {
            if (searchItem != null) {
                searchItem.expandActionView();
            }
            searchView.setIconified(false);
            searchView.setQuery(initialQuery, false);
            updateActionBarButtons(searchView, initialQuery, activity);
        }

        // Clicking the menu item hides the clear button and triggers search for an empty query.
        if (searchItem != null) {
            searchItem.setOnMenuItemClickListener(
                    (MenuItem m) -> {
                        updateActionBarButtons(searchView, "", activity);
                        changeListener.onQueryTextChange("");
                        return false; // Continue with the default action.
                    });
        }

        // Make the close button a clear button.
        findSearchClearButton(searchView)
                .setOnClickListener(
                        (View v) -> {
                            searchView.setQuery("", false);
                            updateActionBarButtons(searchView, "", activity);
                            changeListener.onQueryTextChange("");
                        });

        // Ensure the clear button doesn't reappear with layout changes (e.g. keyboard visibility).
        findSearchClearButton(searchView)
                .addOnLayoutChangeListener(
                        (view, i, i1, i2, i3, i4, i5, i6, i7) ->
                                updateActionBarButtons(
                                        searchView, searchView.getQuery().toString(), activity));

        // Ensure that a changed search view triggers the search - independent from used code path.
        searchView.setOnSearchClickListener(
                view -> {
                    updateActionBarButtons(searchView, "", activity);
                    changeListener.onQueryTextChange("");
                    if (searchViewObserver != null) {
                        searchViewObserver.onUpdated(true);
                    }
                });
        searchView.setOnCloseListener(
                () -> {
                    if (searchViewObserver != null) {
                        searchViewObserver.onUpdated(false);
                    }
                    return false;
                });
        searchView.setOnQueryTextListener(
                new SearchView.OnQueryTextListener() {
                    @Override
                    public boolean onQueryTextSubmit(String query) {
                        return true; // Consume event.
                    }

                    @Override
                    public boolean onQueryTextChange(String query) {
                        updateActionBarButtons(searchView, query, activity);
                        changeListener.onQueryTextChange(query);
                        return true; // Consume event.
                    }
                });
    }

    /**
     * Handles an item in {@link androidx.fragment.app.Fragment#onOptionsItemSelected(MenuItem)} if
     * it is a search item and returns true. If it is not applicable, it returns false.
     * @param selectedItem The user-selected menu item.
     * @param searchItem The menu item known to contain the search view.
     * @param query The current search query.
     * @param activity Optional. If set, overflow icons in the activity's action bar will be hidden.
     * @return Returns true if the item is a search item and could be handled. False otherwise.
     */
    public static boolean handleSearchNavigation(
            MenuItem selectedItem,
            MenuItem searchItem,
            @Nullable String query,
            @Nullable Activity activity) {
        if (selectedItem.getItemId() != android.R.id.home || query == null) return false;
        clearSearch(searchItem, activity);
        return true;
    }

    /**
     * Reset a search item by clearing and collapsing it.
     * @param searchItem The menu item that contains the search item.
     * @param activity Optional. If set, overflow icons in the activity's action bar will be hidden.
     */
    public static void clearSearch(MenuItem searchItem, @Nullable Activity activity) {
        SearchView searchView = (SearchView) searchItem.getActionView();
        assumeNonNull(searchView);
        searchView.setQuery(null, false);
        searchView.setIconified(true);
        searchItem.collapseActionView();
        updateActionBarButtons(searchView, null, activity);
    }

    private static void updateActionBarButtons(
            SearchView searchView, @Nullable String query, @Nullable Activity activity) {
        ImageView clearButton = findSearchClearButton(searchView);
        clearButton.setVisibility(query == null || query.equals("") ? View.GONE : View.VISIBLE);
        if (activity != null) {
            SettingsUtils.setOverflowMenuVisibility(
                    activity, query != null ? View.GONE : View.VISIBLE);
        }
    }

    private static ImageView findSearchClearButton(SearchView searchView) {
        return searchView.findViewById(R.id.search_close_btn);
    }
}
