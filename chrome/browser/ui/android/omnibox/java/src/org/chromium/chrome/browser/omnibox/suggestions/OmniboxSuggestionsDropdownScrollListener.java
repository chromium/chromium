// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

/**
 * Interface that will receive notifications and callbacks when the user scroll the suggestion list.
 */
public interface OmniboxSuggestionsDropdownScrollListener {
    /** Invoked whenever the User scrolls the list. */
    void onSuggestionDropdownScroll();

    /** Invoked whenever the User scrolls the list to the top. */
    void onSuggestionDropdownOverscrolledToTop();
}
