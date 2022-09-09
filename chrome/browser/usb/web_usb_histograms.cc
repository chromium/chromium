// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_histograms.h"

#include "base/metrics/histogram_macros.h"

void RecordWebUsbPermissionRevocation(WebUsbPermissionRevoked kind) {
  UMA_HISTOGRAM_ENUMERATION("WebUsb.PermissionRevoked", kind,
                            WEBUSB_PERMISSION_REVOKED_MAX);
}

void RecordWebUsbChooserClosure(WebUsbChooserClosed disposition) {
  UMA_HISTOGRAM_ENUMERATION("WebUsb.ChooserClosed", disposition,
                            WEBUSB_CHOOSER_CLOSED_MAX);
}
