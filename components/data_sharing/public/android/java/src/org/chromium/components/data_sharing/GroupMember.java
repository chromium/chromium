// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import androidx.annotation.VisibleForTesting;

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
    public final String givenName;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public GroupMember(
            String gaiaId,
            String displayName,
            String email,
            int role,
            GURL avatarUrl,
            String givenName) {
        this.gaiaId = gaiaId;
        this.displayName = displayName;
        this.email = email;
        this.role = role;
        this.avatarUrl = avatarUrl;
        this.givenName = givenName;
    }

    @CalledByNative
    private static GroupMember createGroupMember(
            String gaiaId,
            String displayName,
            String email,
            int role,
            GURL avatarUrl,
            String givenName) {
        return new GroupMember(gaiaId, displayName, email, role, avatarUrl, givenName);
    }
}
