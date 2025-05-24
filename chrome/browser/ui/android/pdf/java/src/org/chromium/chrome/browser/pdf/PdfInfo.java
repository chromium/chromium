// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Simple object representing important information of a pdf native page. */
@NullMarked
public class PdfInfo {
    public final @Nullable String filename;
    public final @Nullable String filepath;
    public final boolean isDownloadSafe;

    public PdfInfo(String filename, @Nullable String filepath, boolean isDownloadSafe) {
        this.filename = filename;
        this.filepath = filepath;
        this.isDownloadSafe = isDownloadSafe;
    }

    public PdfInfo() {
        filename = null;
        filepath = null;
        isDownloadSafe = true;
    }
}
