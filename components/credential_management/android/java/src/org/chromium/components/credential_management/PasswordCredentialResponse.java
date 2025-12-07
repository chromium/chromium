// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.credential_management;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/**
 * A convenience class for conwaying the results of the Credential Management API get() call result
 * to C++.
 */
@NullMarked
@JNINamespace("credential_management")
public class PasswordCredentialResponse {
    private final boolean mSuccess;
    private final String mUsername;
    private final String mPassword;

    @CalledByNative
    public PasswordCredentialResponse(
            @JniType("bool") boolean success,
            @JniType("std::u16string") String username,
            @JniType("std::u16string") String password) {
        mSuccess = success;
        mUsername = username;
        mPassword = password;
    }

    /**
     * @return Whether the call to request the credential was successful.
     */
    @CalledByNative
    public @JniType("bool") boolean getSuccess() {
        return mSuccess;
    }

    /**
     * @return The username.
     */
    @CalledByNative
    public @JniType("std::u16string") String getUsername() {
        return mUsername;
    }

    /**
     * @return The password.
     */
    @CalledByNative
    public @JniType("std::u16string") String getPassword() {
        return mPassword;
    }
}
