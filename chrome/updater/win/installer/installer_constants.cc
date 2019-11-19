// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer/installer_constants.h"

namespace updater {

// The prefix of the updater archive resource.
const wchar_t kUpdaterArchivePrefix[] = L"updater";

// Temp directory prefix that this process creates.
const wchar_t kTempPrefix[] = L"UPDATER";

// 7zip archive.
const wchar_t kLZMAResourceType[] = L"B7";

}  // namespace updater
