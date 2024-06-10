// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth;

public interface IpProtectionByteArrayCallback {
    // Result contains a serialized com.google.privacy.ppn.proto protobuf.
    public void onResult(byte[] result);

    // TODO(b/328780742): update callback error arguments when updating AIDL files
    public void onError(byte[] error);
}
