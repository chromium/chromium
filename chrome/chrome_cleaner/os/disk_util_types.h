// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_DISK_UTIL_TYPES_H_
#define CHROME_CHROME_CLEANER_OS_DISK_UTIL_TYPES_H_

#include <string>

namespace chrome_cleaner {

namespace internal {

// This represents the information gathered on a file.
//
// This struct is related to the FileInformation proto message that is sent in
// reports.  They are kept separate because the data manipulated in the
// cleaner/reporter isn't necessarily the same that we want to transmit to
// Google. Another reason is that protos store strings as UTF-8, whereas the
// functions in base to manipulate user data obtained via the Windows API use
// 16-bits strings.
struct FileInformation {
  FileInformation();

  FileInformation(const std::wstring& path,
                  const std::string& creation_date,
                  const std::string& last_modified_date,
                  const std::string& sha256,
                  int64_t size,
                  const std::wstring& company_name,
                  const std::wstring& company_short_name,
                  const std::wstring& product_name,
                  const std::wstring& product_short_name,
                  const std::wstring& internal_name,
                  const std::wstring& original_filename,
                  const std::wstring& file_description,
                  const std::wstring& file_version,
                  bool active_file = false);

  FileInformation(const FileInformation& other);

  ~FileInformation();

  FileInformation& operator=(const FileInformation& other);

  std::wstring path;
  std::string creation_date;
  std::string last_modified_date;
  std::string sha256;
  int64_t size = 0ULL;
  // The following are internal fields of the PE header.
  std::wstring company_name;
  std::wstring company_short_name;
  std::wstring product_name;
  std::wstring product_short_name;
  std::wstring internal_name;
  std::wstring original_filename;
  std::wstring file_description;
  std::wstring file_version;
  bool active_file = false;
};

}  // namespace internal

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_DISK_UTIL_TYPES_H_
