// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.split_tabs;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.tab_groups.TokenId;

/** Java counterpart to native {@code split_tabs::SplitTabId}. */
@NullMarked
@JNINamespace("split_tabs")
@DoNotMock("This is a simple value object.")
public final class SplitTabId extends TokenId {
    private SplitTabId(Token token) {
        super(token);
    }

    /**
     * Generates a new random {@link SplitTabId}, mirroring native {@code
     * SplitTabId::GenerateNew()}.
     */
    public static SplitTabId generateNew() {
        return new SplitTabId(Token.createRandom());
    }

    /** Wraps an existing {@link Token}, mirroring native {@code SplitTabId::FromRawToken()}. */
    @CalledByNative
    public static SplitTabId fromRawToken(@JniType("base::Token") Token token) {
        return new SplitTabId(token);
    }

    /** Wraps a nullable {@link Token}, mirroring native {@code SplitTabId::FromOptionalToken()}. */
    public static @Nullable SplitTabId fromOptionalToken(@Nullable Token token) {
        return token == null ? null : new SplitTabId(token);
    }

    /**
     * Converts a nullable {@link SplitTabId} to a nullable {@link Token}, mirroring native {@code
     * SplitTabId::ToOptionalToken()}.
     */
    public static @Nullable Token toOptionalToken(@Nullable SplitTabId splitTabId) {
        return splitTabId == null ? null : splitTabId.getToken();
    }
}
