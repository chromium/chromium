// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

import java.util.Objects;

/** Encapsulates provenance information for an Autofill AI entity source. */
@JNINamespace("autofill")
@NullMarked
public final class AutofillAiSourceAttributionInfo {
    private final @SourceType int mSourceType;
    private final GURL mUrl;
    private final String mTitle;

    @CalledByNative
    public AutofillAiSourceAttributionInfo(
            @SourceType int sourceType,
            @JniType("GURL") GURL url,
            @JniType("std::u16string") String title) {
        mSourceType = sourceType;
        mUrl = url;
        mTitle = title;
    }

    @CalledByNative
    public @SourceType int getSourceType() {
        return mSourceType;
    }

    @CalledByNative
    public @JniType("GURL") GURL getUrl() {
        return mUrl;
    }

    @CalledByNative
    public @JniType("std::u16string") String getTitle() {
        return mTitle;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o instanceof AutofillAiSourceAttributionInfo that) {
            return mSourceType == that.mSourceType
                    && Objects.equals(mUrl, that.mUrl)
                    && Objects.equals(mTitle, that.mTitle);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mSourceType, mUrl, mTitle);
    }

    @Override
    public String toString() {
        return "AutofillAiSourceAttributionInfo{"
                + "mSourceType="
                + mSourceType
                + ", mUrl="
                + mUrl
                + ", mTitle='"
                + mTitle
                + '\''
                + '}';
    }
}
