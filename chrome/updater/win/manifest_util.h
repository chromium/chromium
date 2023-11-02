// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_MANIFEST_UTIL_H_
#define CHROME_UPDATER_WIN_MANIFEST_UTIL_H_

#include <string>

namespace base {
class FilePath;
}

namespace updater {

// Parses the offline manifest file and extracts the app install command line.
//
// The function looks for the manifest file "OfflineManifest.gup" in
// `offline_dir`, and falls back to "<app_id>.gup" in the same directory if
// needed.
//
// The manifest file contains the update check response in XML format.
// See https://github.com/google/omaha/blob/master/doc/ServerProtocol.md for
// protocol details.
//
// The function extracts the values from the manifest using a best-effort
// approach. If matching values are found, then:
//   `installer_path`: contains the full path to the app installer.
//   `install_args`: the command line arguments for the app installer.
//   `install_data`: the text value for the key `install_data_index` if such
//                   key/value pair exists in <data> element. During
//                   installation, the text will be serialized to a file and
//                   passed to the app installer.
void ReadInstallCommandFromManifest(const base::FilePath& offline_dir,
                                    const std::string& app_id,
                                    const std::string& install_data_index,
                                    base::FilePath& installer_path,
                                    std::string& install_args,
                                    std::string& install_data);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_MANIFEST_UTIL_H_
