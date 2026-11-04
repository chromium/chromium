// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.password_manager.ode;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.Callback;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.trusted_vault.SecurityDomainId;
import org.chromium.components.trusted_vault.TrustedVaultClient;

/**
 * Helper for checking whether the passkeys ({@code hw_protected}) security domain is empty for a
 * given account.
 */
@NullMarked
public class PasskeySecurityDomainChecker {
    private PasskeySecurityDomainChecker() {}

    /**
     * JNI entry point that delegates to {@link TrustedVaultClient.Backend#getIsSecurityDomainEmpty}
     * for {@link SecurityDomainId#PASSKEYS} and invokes {@code callback} with {@code true}, {@code
     * false}, or {@code null} if the state could not be determined.
     */
    @CalledByNative
    @VisibleForTesting
    static void getIsSecurityDomainEmpty(
            @JniType("CoreAccountInfo") CoreAccountInfo accountInfo,
            @JniType("base::OnceCallback<void(std::optional<bool>)>&&")
                    Callback<@Nullable Boolean> callback) {
        TrustedVaultClient.BackendFactory factory =
                ServiceLoaderUtil.maybeCreate(TrustedVaultClient.BackendFactory.class);
        if (factory == null) {
            callback.onResult(null);
            return;
        }
        factory.createBackend(SecurityDomainId.PASSKEYS)
                .getIsSecurityDomainEmpty(accountInfo)
                .then(result -> callback.onResult(result), exception -> callback.onResult(null));
    }
}
