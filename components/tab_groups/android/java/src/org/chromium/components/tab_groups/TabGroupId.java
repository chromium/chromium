// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_groups;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Java counterpart to native {@code tab_groups::TabGroupId}. */
@NullMarked
@JNINamespace("tab_groups")
@DoNotMock("This is a simple value object.")
public final class TabGroupId extends TokenId {
    private TabGroupId(Token token) {
        super(token);
    }

    /**
     * Generates a new random {@link TabGroupId}, mirroring native {@code
     * TabGroupId::GenerateNew()}.
     */
    public static TabGroupId generateNew() {
        return new TabGroupId(Token.createRandom());
    }

    /** Wraps an existing {@link Token}, mirroring native {@code TabGroupId::FromRawToken()}. */
    @CalledByNative
    public static TabGroupId fromRawToken(@JniType("base::Token") Token token) {
        return new TabGroupId(token);
    }

    /** Wraps a nullable {@link Token}, mirroring native {@code TabGroupId::FromOptionalToken()}. */
    public static @Nullable TabGroupId fromOptionalToken(@Nullable Token token) {
        return token == null ? null : new TabGroupId(token);
    }

    /**
     * Converts a nullable {@link TabGroupId} to a nullable {@link Token}, mirroring native {@code
     * TabGroupId::ToOptionalToken()}.
     */
    public static @Nullable Token toOptionalToken(@Nullable TabGroupId tabGroupId) {
        return tabGroupId == null ? null : tabGroupId.getToken();
    }
}
