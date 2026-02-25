// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import org.chromium.build.annotations.NullMarked;

/** Constants used for Intent extras and communication in ChromeItemPickerActivity. */
@NullMarked
public final class ChromeItemPickerExtras {
    // Prevent instantiation
    private ChromeItemPickerExtras() {}

    /** Intent extra for the list of tab IDs that should be preselected on startup. */
    public static final String EXTRA_PRESELECTED_TAB_IDS = "EXTRA_PRESELECTED_TAB_IDS";

    /** Intent extra indicating if the picker should use Incognito branding/theming. */
    public static final String EXTRA_IS_INCOGNITO_BRANDED = "EXTRA_IS_INCOGNITO_BRANDED";

    /** Intent extra key used when returning the selected tab IDs. */
    public static final String EXTRA_ATTACHMENT_TAB_IDS = "EXTRA_TAB_IDS";

    /** Intent extra defining the maximum number of items a user can select. */
    public static final String EXTRA_ALLOWED_SELECTION_COUNT = "EXTRA_ALLOWED_SELECTION_COUNT";

    /** Intent extra for Single Context Mode (e.g., selecting one unselects others). */
    public static final String EXTRA_IS_SINGLE_CONTEXT_MODE = "EXTRA_IS_SINGLE_CONTEXT_MODE";
}
