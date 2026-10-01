// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import org.chromium.build.annotations.NullMarked;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Configuration containing the list of allowed country codes for bottom bar extra actions.
 *
 * <p><strong>CRITICAL:</strong> All lists of country codes in this file must remain sorted
 * alphabetically. This sorting is strictly enforced by Chromium's keep-sorted presubmit hook.
 *
 * <p>To automatically format and sort this file, run:
 *
 * <pre>
 * git cl format
 * </pre>
 */
@NullMarked
public final class BottomBarGeofencingConfig {
    private BottomBarGeofencingConfig() {}

    /** Countries where GLIC (Gemini) is allowed. */
    public static final List<String> GLIC_ALLOWED_COUNTRIES =
            Collections.unmodifiableList(
                    Arrays.asList(
                            // keep-sorted start
                            "in", // India
                            "us" // United States
                            // keep-sorted end
                            ));
}
