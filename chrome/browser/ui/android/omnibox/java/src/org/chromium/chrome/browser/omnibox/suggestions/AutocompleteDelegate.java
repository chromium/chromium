// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.ui.base.PageTransition;

/** Provides the additional functionality to trigger and interact with autocomplete suggestions. */
public interface AutocompleteDelegate extends UrlBarDelegate {
    /** Notified that the URL text has changed. */
    void onUrlTextChanged();

    /**
     * Notified that suggestions have changed.
     *
     * @param autocompleteText The inline autocomplete text that can be appended to the currently
     *     entered user text.
     * @param defaultMatchIsSearch Whether the default match is a search (as opposed to a URL). This
     *     is true if there are no suggestions.
     */
    void onSuggestionsChanged(String autocompleteText, boolean defaultMatchIsSearch);

    /**
     * Requests the keyboard visibility update.
     *
     * @param shouldShow When true, keyboard should be made visible.
     * @param delayHide when true, hiding will commence after brief delay.
     */
    void setKeyboardVisibility(boolean shouldShow, boolean delayHide);

    /**
     * @return Reports whether keyboard (whether software or hardware) is active. Software keyboard
     *     is reported as active whenever it is visible on screen; hardware keyboard is reported as
     *     active when it is connected.
     */
    boolean isKeyboardActive();

    /**
     * Requests that the given URL be loaded in the current tab or in a new tab.
     *
     * @param url The URL to be loaded.
     * @param transition The transition type associated with the url load.
     * @param inputStart The time the input started for the load request.
     * @param openInNewTab Whether the URL will be loaded in a new tab. If {@code true}, the URL
     *     will be loaded in a new tab. If {@code false}, The URL will be loaded in the current tab.
     */
    void loadUrl(String url, @PageTransition int transition, long inputStart, boolean openInNewTab);

    /**
     * Requests that the given URL be loaded in the current tab.
     *
     * @param url The URL to be loaded.
     * @param transition The transition type associated with the url load.
     * @param inputStart The time the input started for the load request.
     * @param postDataType postData type.
     * @param postData Post-data to include in the tab URL's request body, ex. bitmap when image
     *     search.
     */
    void loadUrlWithPostData(
            String url,
            @PageTransition int transition,
            long inputStart,
            String postDataType,
            byte[] postData);

    /**
     * @return Whether the omnibox was focused via the NTP fakebox.
     */
    boolean didFocusUrlFromFakebox();

    /**
     * @return Whether the URL currently has focus.
     */
    boolean isUrlBarFocused();
}
