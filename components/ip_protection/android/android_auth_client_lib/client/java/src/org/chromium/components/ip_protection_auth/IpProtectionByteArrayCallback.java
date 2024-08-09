// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth;

import androidx.annotation.NonNull;

public interface IpProtectionByteArrayCallback {
    // Result contains a serialized com.google.privacy.ppn.proto protobuf.
    public void onResult(@NonNull byte[] result);

    // Error is an auth request error defined in IpProtectionAuthClent.AuthRequestError
    public void onError(int authRequestError);
}
