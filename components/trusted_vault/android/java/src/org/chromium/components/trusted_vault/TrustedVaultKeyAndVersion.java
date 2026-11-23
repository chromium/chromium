// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.trusted_vault;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;
import java.util.Objects;

/** Represents a trusted vault encryption key paired with its version (epoch). */
@NullMarked
public final class TrustedVaultKeyAndVersion {
    /** Raw shared key bytes. */
    public final byte[] key;

    /** Key version (epoch) assigned by the security domain service. */
    public final int version;

    public TrustedVaultKeyAndVersion(byte[] key, int version) {
        this.key = key;
        this.version = version;
    }

    @CalledByNative
    @JniType("std::vector<uint8_t>")
    byte[] getKey() {
        return key;
    }

    @CalledByNative
    int getVersion() {
        return version;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        return obj instanceof TrustedVaultKeyAndVersion other
                && Arrays.equals(this.key, other.key)
                && this.version == other.version;
    }

    @Override
    public int hashCode() {
        return Objects.hash(Arrays.hashCode(this.key), this.version);
    }
}
