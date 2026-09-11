// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Holds metadata for a Drive file attached to a contextual search query. */
@NullMarked
public class DriveAttachmentMetadata {
    public final String driveId;
    public final @Nullable String resourceKey;
    public final String title;
    public final String mimeType;

    public DriveAttachmentMetadata(
            String driveId, @Nullable String resourceKey, String title, String mimeType) {
        this.driveId = driveId;
        this.resourceKey = resourceKey;
        this.title = title;
        this.mimeType = mimeType;
    }
}
