// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.Origin;

/** Simple object representing important information of a pdf native page. */
@NullMarked
public class PdfInfo {
    public final @Nullable String filename;
    public final @Nullable String filepath;
    public final boolean isDownloadSafe;
    public final boolean preferReuse;
    public final @Nullable Origin initiatorOrigin;

    public static PdfInfo initReuse(boolean preferReuse) {
        return initReuse(preferReuse, /* initiatorOrigin= */ null);
    }

    public static PdfInfo initReuse(boolean preferReuse, @Nullable Origin initiatorOrigin) {
        return new PdfInfo(null, null, true, preferReuse, initiatorOrigin);
    }

    private PdfInfo(
            @Nullable String filename,
            @Nullable String filepath,
            boolean isDownloadSafe,
            boolean preferReuse,
            @Nullable Origin initiatorOrigin) {
        this.filename = filename;
        this.filepath = filepath;
        this.isDownloadSafe = isDownloadSafe;
        this.preferReuse = preferReuse;
        this.initiatorOrigin = initiatorOrigin;
    }

    public PdfInfo(
            @Nullable String filename,
            @Nullable String filepath,
            boolean isDownloadSafe,
            @Nullable Origin initiatorOrigin) {
        this(filename, filepath, isDownloadSafe, false, initiatorOrigin);
    }

    public PdfInfo(@Nullable String filename, @Nullable String filepath, boolean isDownloadSafe) {
        this(filename, filepath, isDownloadSafe, false, /* initiatorOrigin= */ null);
    }

    public PdfInfo() {
        this(null, null, true, false, /* initiatorOrigin= */ null);
    }
}
