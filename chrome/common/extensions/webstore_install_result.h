// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_WEBSTORE_INSTALL_RESULT_H_
#define CHROME_COMMON_EXTENSIONS_WEBSTORE_INSTALL_RESULT_H_

namespace extensions {

namespace webstore_install {

extern const char kInvalidWebstoreItemId[];
extern const char kWebstoreRequestError[];
extern const char kInvalidWebstoreResponseError[];
extern const char kInvalidManifestError[];
extern const char kUserCancelledError[];
extern const char kExtensionIsBlocklisted[];
extern const char kInstallInProgressError[];

// Result codes returned by WebstoreStandaloneInstaller and its subclasses.
enum Result {
  // Successful operation.
  SUCCESS,

  // Unknown error.
  OTHER_ERROR,

  // The operation was aborted as the requestor is no longer alive.
  ABORTED,

  // An installation of the same extension is in progress.
  INSTALL_IN_PROGRESS,

  // The installation is not permitted.
  NOT_PERMITTED,

  // Invalid Chrome Web Store item ID.
  INVALID_ID,

  // Failed to retrieve extension metadata from the Web Store.
  WEBSTORE_REQUEST_ERROR,

  // The extension metadata retrieved from the Web Store was invalid.
  INVALID_WEBSTORE_RESPONSE,

  // An error occurred while parsing the extension manifest retrieved from the
  // Web Store.
  INVALID_MANIFEST,

  // Failed to retrieve the extension's icon from the Web Store, or the icon
  // was invalid.
  ICON_ERROR,

  // The user cancelled the operation.
  USER_CANCELLED,

  // The extension is blocklisted.
  BLOCKLISTED,

  // Unsatisfied dependencies, such as shared modules.
  MISSING_DEPENDENCIES,

  // Unsatisfied requirements, such as webgl.
  REQUIREMENT_VIOLATIONS,

  // The extension is blocked by management policies.
  BLOCKED_BY_POLICY,

  // The launch feature is not available.
  LAUNCH_FEATURE_DISABLED,

  // The launch feature is not supported for the extension type.
  LAUNCH_UNSUPPORTED_EXTENSION_TYPE,

  // A launch of the same extension is in progress.
  LAUNCH_IN_PROGRESS,

  // The final (and unused) result type for enum verification.
  // New results should go above this entry, and this entry should be updated.
  RESULT_LAST = LAUNCH_IN_PROGRESS,
};

}  // namespace webstore_install

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_WEBSTORE_INSTALL_RESULT_H_
