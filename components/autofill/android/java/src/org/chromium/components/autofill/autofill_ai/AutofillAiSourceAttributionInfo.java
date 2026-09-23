// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/** Encapsulates provenance information for an Autofill AI entity source. */
@NullMarked
public final class AutofillAiSourceAttributionInfo {
    private final @SourceType int mSourceType;
    private final GURL mUrl;
    private final String mTitle;

    public AutofillAiSourceAttributionInfo(@SourceType int sourceType, GURL url, String title) {
        mSourceType = sourceType;
        mUrl = url;
        mTitle = title;
    }

    public @SourceType int getSourceType() {
        return mSourceType;
    }

    public GURL getUrl() {
        return mUrl;
    }

    public String getTitle() {
        return mTitle;
    }
}
