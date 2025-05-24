// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.base.GaiaId;
import org.chromium.url.GURL;

import java.util.Objects;

/** Information about a member of a group. */
@JNINamespace("data_sharing")
@NullMarked
public class GroupMember {
    public final GaiaId gaiaId;
    public final String displayName;
    public final String email;
    public final @MemberRole int role;
    public final GURL avatarUrl;
    public final String givenName;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public GroupMember(
            GaiaId gaiaId,
            String displayName,
            String email,
            @MemberRole int role,
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
            GaiaId gaiaId,
            String displayName,
            String email,
            @MemberRole int role,
            GURL avatarUrl,
            String givenName) {
        return new GroupMember(gaiaId, displayName, email, role, avatarUrl, givenName);
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;

        if (o instanceof GroupMember other) {
            return Objects.equals(this.gaiaId, other.gaiaId)
                    && Objects.equals(this.displayName, other.displayName)
                    && Objects.equals(this.email, other.email)
                    && this.role == other.role
                    && Objects.equals(this.avatarUrl, other.avatarUrl)
                    && Objects.equals(this.givenName, other.givenName);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                this.gaiaId,
                this.displayName,
                this.email,
                this.role,
                this.avatarUrl,
                this.givenName);
    }
}
