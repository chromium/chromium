// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.url.GURL;

/** Information about a member of a group. */
@JNINamespace("data_sharing")
public class GroupMember {
    public final String gaiaId;
    public final String displayName;
    public final String email;
    public final @MemberRole int role;
    public final GURL avatarUrl;

    GroupMember(String gaiaId, String displayName, String email, int role, GURL avatarUrl) {
        this.gaiaId = gaiaId;
        this.displayName = displayName;
        this.email = email;
        this.role = role;
        this.avatarUrl = avatarUrl;
    }

    @CalledByNative
    private static GroupMember createGroupMember(
            String gaiaId, String displayName, String email, int role, GURL avatarUrl) {
        return new GroupMember(gaiaId, displayName, email, role, avatarUrl);
    }
}
