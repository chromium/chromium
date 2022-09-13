// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_CSV_WRITER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_CSV_WRITER_H_

#include <map>
#include <string>
#include <vector>

namespace password_manager {

// Writes tabular data into CSV (Comma Separated Values) format as defined in
// RFC 4180, with the following caveats:
//   * The output encoding will be UTF-8. No code points will be escaped.
//   * Lines will be separated by the platform-specific EOL terminator.
//   * Values will be enclosed in double-quotes if and only if they contain
//     either of the following characters: CR, LF, double-quote ("), comma (,).
//
// The CSV representation of the supplied data will be generated into |csv|,
// which is first cleared of pre-existing data. The first line of the file will
// contain |column_names|, followed by as many lines as there are elements in
// |records|. Each element in |records| should be a dictionary that maps column
// names to values, so they can be output in the right order. All passed in
// strings should be UTF-8 encoded.
void WriteCSV(const std::vector<std::string>& column_names,
              const std::vector<std::map<std::string, std::string>>& records,
              std::string* csv);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_EXPORT_CSV_WRITER_H_
