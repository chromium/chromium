// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_INSTALLER_INSTALLER_CONSTANTS_H_
#define CHROME_UPDATER_WIN_INSTALLER_INSTALLER_CONSTANTS_H_

namespace updater {

// Various filenames and prefixes.
inline constexpr wchar_t kUpdaterArchivePrefix[] = L"updater";

// The resource types that would be unpacked from the mini installer.
inline constexpr wchar_t kLZMAResourceType[] = L"B7";

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_INSTALLER_INSTALLER_CONSTANTS_H_
