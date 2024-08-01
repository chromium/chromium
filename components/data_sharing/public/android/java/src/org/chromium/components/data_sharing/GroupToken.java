// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Information about a member of a group. */
@JNINamespace("data_sharing")
public class GroupToken {
    public final String groupId;
    public final String accessToken;

    /**
     * Constructor for a {@link GroupToken} object.
     *
     * @param groupId The ID associated with the group.
     * @param accessToken The access token associated with the group.
     */
    public GroupToken(String groupId, String accessToken) {
        this.groupId = groupId;
        this.accessToken = accessToken;
    }

    @CalledByNative
    private static GroupToken createGroupToken(String groupId, String accessToken) {
        return new GroupToken(groupId, accessToken);
    }
}
