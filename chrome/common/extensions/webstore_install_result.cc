// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/webstore_install_result.h"

namespace extensions {
namespace webstore_install {

const char kInvalidWebstoreItemId[] = "Invalid Chrome Web Store item ID";
const char kWebstoreRequestError[] =
    "Could not fetch data from the Chrome Web Store";
const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";
const char kInvalidManifestError[] = "Invalid manifest";
const char kUserCancelledError[] = "User cancelled install";
const char kExtensionIsBlocklisted[] = "Extension is blocklisted";
const char kInstallInProgressError[] = "An install is already in progress";

}  // namespace webstore_install
}  // namespace extensions
