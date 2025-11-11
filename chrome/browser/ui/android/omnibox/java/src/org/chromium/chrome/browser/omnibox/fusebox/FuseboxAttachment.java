// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assertNonNull;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;

/** A complete fusebox attachment with all data and metadata. */
@NullMarked
public final class FuseboxAttachment {
    public final @FuseboxAttachmentType int itemType;
    public final @Nullable Drawable thumbnail;
    public final String title;
    public final String mimeType;
    public final byte[] data;
    private @Nullable String mToken;

    public FuseboxAttachment(
            @FuseboxAttachmentType int itemType,
            @Nullable Drawable thumbnail,
            String title,
            String mimeType,
            byte[] data) {
        this.itemType = itemType;
        this.thumbnail = thumbnail;
        this.title = title;
        this.mimeType = mimeType;
        this.data = data;
        mToken = null;
    }

    /** Sets the token for this attachment. */
    /* package */ void setToken(String token) {
        mToken = token;
    }

    /** Gets the token for this attachment. */
    /* package */ String getToken() {
        return assertNonNull(mToken);
    }
}
