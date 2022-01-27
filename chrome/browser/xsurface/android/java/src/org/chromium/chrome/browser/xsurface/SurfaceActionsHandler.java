// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;

import java.util.List;
/**
 * Interface to provide chromium calling points for an external surface.
 */
public interface SurfaceActionsHandler {
    String KEY = "GeneralActions";

    /**
     * Navigates the current tab to a particular URL.
     * @param url The url for which to navigate.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    default void navigateTab(String url, View actionSourceView) {}

    /**
     * Navigates a new tab to a particular URL.
     * @param url The url for which to navigate.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    default void navigateNewTab(String url, View actionSourceView) {}

    /**
     * Navigate a new incognito tab to a URL.
     */
    default void navigateIncognitoTab(String url) {}

    /**
     * Get an offline page for a URL.
     */
    default void downloadLink(String url) {}

    /** Add the url to the reading list and make it available offline. */
    default void addToReadingList(String title, String url) {}

    /**
     * Open a bottom sheet with the view as contents.
     * @param view The bottom sheet contents view.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    default void showBottomSheet(View view, View actionSourceView) {}

    /**
     * Dismiss the open bottom sheet (or do nothing if there isn't one).
     *
     */
    default void dismissBottomSheet() {}

    /**
     * Notifies the host app that url with broadTopicMids and entityMids was clicked.
     * @param url The URL that the user clicked on
     * @param entityMids Sorted list (most relevant to least) of entity MIDs that correspond to the
     *         clicked URL
     */
    default void updateUserProfileOnLinkClick(String url, List<Long> entityMids) {}

    /**
     * Attempts to follow a WebFeed. If the WebFeed cannot be followed immediately
     * due to network limitations, the operation will be retried at a later time.
     */
    default void followWebFeed(String webFeedName) {}

    /**
     * Attempts to unfollow a WebFeed. If the WebFeed cannot be unfollowed immediately
     * due to network limitations, the operation will be retried at a later time.
     */
    default void unfollowWebFeed(String webFeedName) {}
}
