// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Implemented in Chromium.
 *
 * Interface to provide chromium calling points for an external surface.
 */
@NullMarked
public interface SurfaceActionsHandler {
    String KEY = "GeneralActions";

    @IntDef({
        OpenMode.UNKNOWN,
        OpenMode.SAME_TAB,
        OpenMode.NEW_TAB,
        OpenMode.INCOGNITO_TAB,
        OpenMode.DOWNLOAD_LINK,
        OpenMode.READ_LATER,
        OpenMode.THANK_CREATOR,
        OpenMode.NEW_TAB_IN_GROUP
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface OpenMode {
        int UNKNOWN = 0;
        // The URL is opened in the same tab (default).
        int SAME_TAB = 1;
        // The URL is opened in a new tab.
        int NEW_TAB = 2;
        // The URL is opened in an incognito tab.
        int INCOGNITO_TAB = 3;
        // The URL is downloaded.
        int DOWNLOAD_LINK = 4;
        // The URL is added for later reading.
        int READ_LATER = 5;
        // Deprecated. The URL to thank the current creator is opened in a Chrome Custom Tab (CCT).
        int THANK_CREATOR = 6;
        // The URL is opened in a new tab that is organized as group.
        int NEW_TAB_IN_GROUP = 7;
    }

    /** Options when opening URLs with openUrl(). */
    interface OpenUrlOptions {
        /** Returns the title. Currently used only for READ_LATER. */
        default String getTitle() {
            return "";
        }

        /** The View from which the user tap originated. May be null.*/
        default @Nullable View actionSourceView() {
            return null;
        }
    }

    /**
     * Navigates the current tab to a particular URL.
     * @param openMode The OpenMode to use.
     * @param url The url for which to navigate.
     * @param options Additional options for opening the URL.
     */
    default void openUrl(@OpenMode int openMode, String url, OpenUrlOptions options) {}

    /**
     * Navigates the current tab to a particular URL.
     * @param url The url for which to navigate.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    @Deprecated
    default void navigateTab(String url, View actionSourceView) {}

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
     *
     * @param url The URL that the user clicked on
     * @param entityMids Sorted list (most relevant to least) of entity MIDs that correspond to the
     *     clicked URL
     */
    default void updateUserProfileOnLinkClick(String url, List<Long> entityMids) {}

    default void updateUserProfileOnLinkClick(
            String url, List<Long> entityMids, long contentCategoryMediaType, long cardCategory) {}

    /**
     * Requests that sign-in flow be started.
     *
     * @deprecated Use startSigninFlow() instead.
     */
    @Deprecated
    default void showSyncConsentPrompt() {}

    /** Requests that sign-in flow be started. */
    default void startSigninFlow() {}

    /** Requests that a sign-in interstitial bottom sheet be shown. */
    default void showSignInInterstitial() {}
}
