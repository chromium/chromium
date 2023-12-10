// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

/** Handles interaction with other UI's when a bottom sheet goes in and out of expanded mode. */
public interface ExpandedSheetHelper {
    /** Sheet gets expanded. */
    void onSheetExpanded();

    /** Sheet gets collapsed. */
    void onSheetCollapsed();
}
