// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.util.List;

/** Information about a group. */
@JNINamespace("data_sharing")
public class GroupData {
    public final String groupId;
    public final String displayName;
    public final List<GroupMember> members;

    GroupData(String groupId, String displayName, GroupMember[] members) {
        this.groupId = groupId;
        this.displayName = displayName;
        this.members = members == null ? null : List.of(members);
    }

    @CalledByNative
    private static GroupData createGroupData(
            String groupId, String displayName, GroupMember[] members) {
        return new GroupData(groupId, displayName, members);
    }
}
