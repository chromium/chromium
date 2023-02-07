// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/launcher_constants.h"

#include <stdbool.h>

#include "chrome/updater/mac/launcher_version.h"
#include "chrome/updater/updater_branding.h"

const char kBundlePath[] =
    "/Library/Application Support/" COMPANY_SHORTNAME_STRING
    "/" PRODUCT_FULLNAME_STRING "/" PRODUCT_VERSION "/" PRODUCT_FULLNAME_STRING
    ".app";
const char kExecutablePath[] =
    "/Library/Application Support/" COMPANY_SHORTNAME_STRING
    "/" PRODUCT_FULLNAME_STRING "/" PRODUCT_VERSION "/" PRODUCT_FULLNAME_STRING
    ".app/Contents/MacOS/" PRODUCT_FULLNAME_STRING;
const char kExecutableName[] = PRODUCT_FULLNAME_STRING;

const bool kCheckSigning = true;
