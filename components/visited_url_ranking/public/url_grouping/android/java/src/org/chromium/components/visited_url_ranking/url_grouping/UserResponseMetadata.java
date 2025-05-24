// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.visited_url_ranking.url_grouping;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/** Container class needed to pass information from Native to Java and vice versa. */
@NullMarked
public class UserResponseMetadata {
    public final int mSuggestionId;
    public final @UserResponse int mUserResponse;

    public UserResponseMetadata(int suggestionId, @UserResponse int userResponse) {
        mSuggestionId = suggestionId;
        mUserResponse = userResponse;
    }

    @CalledByNative
    public int getSuggestionId() {
        return mSuggestionId;
    }

    @CalledByNative
    public @UserResponse int getUserResponse() {
        return mUserResponse;
    }

    @CalledByNative
    private static UserResponseMetadata create(int suggestionId, @UserResponse int userResponse) {
        return new UserResponseMetadata(suggestionId, userResponse);
    }
}
