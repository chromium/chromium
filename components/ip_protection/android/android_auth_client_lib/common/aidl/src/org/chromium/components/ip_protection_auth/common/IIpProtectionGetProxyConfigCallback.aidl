// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.common;

/**
 * Used for communicating the result of getProxyConfig() to the caller.
 */
oneway interface IIpProtectionGetProxyConfigCallback {
    // Parameter is the serialized form of GetProxyConfigResponse proto
    void reportResult(in byte[] response) = 0;
    // errorCode is one of the error constants in IErrorCode.
    void reportError(in int errorCode) = 1;
}
