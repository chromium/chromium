// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties required to build the PDF selection menu. */
@NullMarked
class PdfSelectionProperties {
    /** The callback for context menu actions. */
    static final WritableObjectPropertyKey<PdfSelectionCoordinator.SelectionMenuItemPreparer>
            SELECTION_MENU_ITEM_PREPARER = new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {SELECTION_MENU_ITEM_PREPARER};
}
