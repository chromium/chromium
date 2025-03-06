// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import org.chromium.build.annotations.NullMarked;

/** Contains all of the command line switches that are specific to the context menu. */
@NullMarked
public final class ContextMenuSwitches {
    // Force enable the popup context menu to unblock drag and drop from web
    // contents on phones.
    public static final String FORCE_CONTEXT_MENU_POPUP = "force-context-menu-popup";

    // Prevents instantiation.
    private ContextMenuSwitches() {}
}
