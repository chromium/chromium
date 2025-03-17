// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.chromium.chrome.browser.tab.TabLaunchType;

/**
 * GroupSuggestionsService is the core class for managing group suggestions. It represents a native
 * GroupSuggestionsService object in Java.
 */
public interface GroupSuggestionsService {
    /** Gets called when a tab is added. */
    void didAddTab(int tabId, @TabLaunchType int type);
}
