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

/**
 * Base class for {@link Token}-backed identifiers (such as {@link TabGroupId}), mirroring native
 * {@code tab_groups::TokenId<T>}.
 */
@NullMarked
@JNINamespace("tab_groups")
@DoNotMock("This is a simple value object.")
public abstract class TokenId {
    private final Token mToken;

    protected TokenId(Token token) {
        mToken = token;
    }

    /** Returns the underlying {@link Token}, mirroring native {@code TokenId::token()}. */
    @CalledByNative
    @JniType("base::Token")
    public final Token getToken() {
        return mToken;
    }

    /**
     * Returns whether the underlying {@link Token} is zero, mirroring native {@code
     * TokenId::is_empty()}.
     */
    public final boolean isEmpty() {
        return mToken.isZero();
    }

    @SuppressWarnings("EqualsGetClass")
    @Override
    public final boolean equals(@Nullable Object obj) {
        if (this == obj) {
            return true;
        }

        if (obj == null) {
            return false;
        }

        // Exact runtime class comparison is intentional so distinct TokenId subclasses (e.g.
        // TabGroupId vs. SplitTabId) are never equal even when wrapping identical Token values.
        if (getClass() != obj.getClass()) {
            return false;
        }

        TokenId other = (TokenId) obj;
        return mToken.equals(other.mToken);
    }

    @Override
    public final int hashCode() {
        return mToken.hashCode();
    }

    @Override
    public final String toString() {
        return mToken.toString();
    }
}
