// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;

/** Data class representing a group of tabs in a card in the Tab Switcher. */
public class TabBin {
    public final List<Tab> tabs;
    public final @Nullable Token groupId;
    public final int rootId;

    /**
     * Constructor. Expects to receive a non-null group ID for tab groups. Otherwise pass a null to
     * indicate a lone tab.
     */
    public TabBin(@Nullable Token groupId, int rootId) {
        this.tabs = new ArrayList<>();
        this.groupId = groupId;
        this.rootId = rootId;
    }
}
