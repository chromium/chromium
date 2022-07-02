// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code;

/** Delegate interface for Assistant QR Code Scan actions. */
public interface AssistantQrCodeDelegate {
    /** Called when QR Code Scan is successfully completed. */
    public void onScanResult(String value);

    /** Called when QR Code Scan is cancelled. */
    public void onScanCancelled();

    /** Called when QR Code Scan fails because of Camera Error. */
    public void onCameraError();
}
