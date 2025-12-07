// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.AccountInfo;

/** A utility class to serialize AccountInfo objects to and from JSON strings. */
@JNINamespace("signin")
@NullMarked
final class AccountInfoSerializer {
    private AccountInfoSerializer() {}

    /**
     * Serializes an AccountInfo object to a JSON string.
     *
     * @param accountInfo The AccountInfo object to serialize.
     * @return A JSON string or null if serialization fails.
     */
    static @Nullable String toJsonString(AccountInfo accountInfo) {
        return AccountInfoSerializerJni.get().accountInfoToJsonString(accountInfo);
    }

    /**
     * Deserializes an AccountInfo object from a JSON string.
     *
     * @param jsonString The JSON string to deserialize.
     * @return An AccountInfo object or null if deserialization fails.
     */
    static @Nullable AccountInfo fromJsonString(String jsonString) {
        return AccountInfoSerializerJni.get().jsonStringToAccountInfo(jsonString);
    }

    /**
     * Native methods are implemented in
     * components/signin/internal/identity_manager/account_info_serializer.cc.
     */
    @NativeMethods
    interface Natives {
        @Nullable
        String accountInfoToJsonString(AccountInfo accountInfo);

        @Nullable
        AccountInfo jsonStringToAccountInfo(@JniType("std::string") String jsonString);
    }
}
