// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Information about a member of a group. */
@JNINamespace("data_sharing")
@NullMarked
public class GroupToken {
    @Deprecated(since = "Use collaborationId instead")
    public final String groupId;

    public final String collaborationId;
    public final @Nullable String accessToken;

    /**
     * Constructor for a {@link GroupToken} object.
     *
     * @param collaborationId The sharing ID associated with the group.
     * @param accessToken The access token associated with the group.
     */
    public GroupToken(String collaborationId, @Nullable String accessToken) {
        this.groupId = collaborationId;
        this.collaborationId = collaborationId;
        this.accessToken = accessToken;
    }

    @CalledByNative
    private static GroupToken createGroupToken(String collaborationId, String accessToken) {
        return new GroupToken(collaborationId, accessToken);
    }
}
