// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.JniOnceCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Container for cached group suggestions. */
@JNINamespace("visited_url_ranking")
@NullMarked
public class CachedSuggestions {
    public final @Nullable GroupSuggestions groupSuggestions;

    // This callback must be called once or destroyed before it goes out of scope.
    public final JniOnceCallback<UserResponseMetadata> userResponseMetadataCallback;

    public CachedSuggestions(
            @Nullable GroupSuggestions suggestions,
            JniOnceCallback<UserResponseMetadata> callback) {
        this.groupSuggestions = suggestions;
        this.userResponseMetadataCallback = callback;
    }

    @CalledByNative
    private static CachedSuggestions create(
            @Nullable GroupSuggestions suggestions,
            JniOnceCallback<UserResponseMetadata> callback) {
        return new CachedSuggestions(suggestions, callback);
    }
}
