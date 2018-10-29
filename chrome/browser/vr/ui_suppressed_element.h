// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_SUPPRESSED_ELEMENT_H_
#define CHROME_BROWSER_VR_UI_SUPPRESSED_ELEMENT_H_

namespace vr {

// When adding values, insert them before kCount and add them to
// VRSuppressedElement in enums.xml. Do not reuse values.
// Also, remove kPlaceholderForPreviousHighValue.
// When values become obsolete, comment them out here and mark them deprecated
// in enums.xml.
enum class UiSuppressedElement : int {
  kFileChooser = 0,
  kBluetoothChooser = 1,
  // kJavascriptDialog = 2,
  // kMediaPermission = 3,
  // kPermissionRequest = 4,
  // kQuotaPermission = 5,
  kHttpAuth = 6,
  // kDownloadPermission = 7,
  kFileAccessPermission = 8,
  kPasswordManager = 9,
  // kAutofill = 10,
  kUsbChooser = 11,
  kSslClientCertificate = 12,
  kMediaRouterPresentationRequest = 13,
  kContextMenu = 14,
  // kPermissionBubbleRequest = 15,
  // TODO(sumankancherla): Remove this placeholder when adding a new value.
  kPlaceholderForPreviousHighValue = 15,
  // This must be the last.
  kCount,
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_SUPPRESSED_ELEMENT_H_
