// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.share;

/** Constants used for {@link ClipboardImageFileProvider}. */
public final class ClipboardConstants {
    /**
     * The preference keys for the last URI shared via the Android system clibpoard and the
     * timestamp of when that happened.
     *
     * Note: We're using the 'Chrome' prefix here as this code came from //chrome and renaming these
     * preferences would have been problematic for existing clients. Make sure to have a migration
     * plan when updating these in future.
     */
    public static final String CLIPBOARD_SHARED_URI = "Chrome.Clipboard.SharedUri";

    public static final String CLIPBOARD_SHARED_URI_TIMESTAMP =
            "Chrome.Clipboard.SharedUriTimestamp";
}
