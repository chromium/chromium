// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.common;

import org.chromium.components.ip_protection_auth.common.IIpProtectionAuthAndSignCallback;
import org.chromium.components.ip_protection_auth.common.IIpProtectionGetInitialDataCallback;

/**
 * Used for conversing with the IP Protection Service.
 */
interface IIpProtectionAuthService {
  // Request should be the serialized form of GetInitialDataRequest proto
  void getInitialData(in byte[] request, in IIpProtectionGetInitialDataCallback callback) = 0;
  // Request should be the serialized form of AuthAndSignRequest proto
  void authAndSign(in byte[] request, in IIpProtectionAuthAndSignCallback callback) = 1;
}
