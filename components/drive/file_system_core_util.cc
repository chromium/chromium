// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/file_system_core_util.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/drive/drive.pb.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/job_list.h"
#include "components/prefs/pref_service.h"

namespace drive {
namespace util {

namespace {

std::string ReadStringFromGDocFile(const base::FilePath& file_path,
                                   const std::string& key) {
  const int64_t kMaxGDocSize = 4096;
  int64_t file_size = 0;
  if (!base::GetFileSize(file_path, &file_size) || file_size > kMaxGDocSize) {
    LOG(WARNING) << "File too large to be a GDoc file " << file_path.value();
    return std::string();
  }

  JSONFileValueDeserializer reader(file_path);
  std::string error_message;
  std::unique_ptr<base::Value> root_value(
      reader.Deserialize(nullptr, &error_message));
  if (!root_value) {
    LOG(WARNING) << "Failed to parse " << file_path.value() << " as JSON."
                 << " error = " << error_message;
    return std::string();
  }

  base::DictionaryValue* dictionary_value = nullptr;
  std::string result;
  if (!root_value->GetAsDictionary(&dictionary_value) ||
      !dictionary_value->GetString(key, &result)) {
    LOG(WARNING) << "No value for the given key is stored in "
                 << file_path.value() << ". key = " << key;
    return std::string();
  }

  return result;
}

}  // namespace

const base::FilePath& GetDriveGrandRootPath() {
  static base::NoDestructor<base::FilePath> grand_root_path(
      base::FilePath::FromUTF8Unsafe(kDriveGrandRootDirName));
  return *grand_root_path;
}

const base::FilePath& GetDriveMyDriveRootPath() {
  static base::NoDestructor<base::FilePath> drive_root_path(
      GetDriveGrandRootPath().AppendASCII(kDriveMyDriveRootDirName));
  return *drive_root_path;
}

const base::FilePath& GetDriveTeamDrivesRootPath() {
  static base::NoDestructor<base::FilePath> team_drives_root_path(
      GetDriveGrandRootPath().AppendASCII(kDriveTeamDrivesDirName));
  return *team_drives_root_path;
}

bool IsTeamDrivesPath(const base::FilePath& file_path) {
  return GetDriveTeamDrivesRootPath().IsParent(file_path);
}

std::string EscapeCacheFileName(const std::string& filename) {
  // This is based on net/base/escape.cc: net::(anonymous namespace)::Escape
  std::string escaped;
  for (size_t i = 0; i < filename.size(); ++i) {
    char c = filename[i];
    if (c == '%' || c == '.' || c == '/') {
      base::StringAppendF(&escaped, "%%%02X", c);
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

std::string UnescapeCacheFileName(const std::string& filename) {
  std::string unescaped;
  for (size_t i = 0; i < filename.size(); ++i) {
    char c = filename[i];
    if (c == '%' && i + 2 < filename.length()) {
      c = (base::HexDigitToInt(filename[i + 1]) << 4) +
          base::HexDigitToInt(filename[i + 2]);
      i += 2;
    }
    unescaped.push_back(c);
  }
  return unescaped;
}

std::string ConvertChangestampToStartPageToken(int64_t changestamp) {
  DCHECK_LE(0, changestamp);
  return base::NumberToString(changestamp + 1);
}

// Convers a start page token to a numerical changestamp
bool ConvertStartPageTokenToChangestamp(const std::string& start_page_token,
                                        int64_t* changestamp) {
  DCHECK(changestamp);
  int64_t result;
  if (base::StringToInt64(start_page_token, &result)) {
    // The minimum valid start_page_token is 1.
    if (result > 0) {
      *changestamp = result - 1;
      return true;
    }
  }
  return false;
}

std::string NormalizeFileName(const std::string& input) {
  DCHECK(base::IsStringUTF8(input));

  std::string output;
  if (!base::ConvertToUtf8AndNormalize(input, base::kCodepageUTF8, &output))
    output = input;
  base::ReplaceChars(output, "/", "_", &output);
  if (!output.empty() && output.find_first_not_of('.', 0) == std::string::npos)
    output = "_";
  return output;
}

bool CreateGDocFile(const base::FilePath& file_path,
                    const GURL& url,
                    const std::string& resource_id) {
  std::string content =
      base::StringPrintf(R"({"url": "%s", "resource_id": "%s"})",
                         url.spec().c_str(), resource_id.c_str());
  return base::WriteFile(file_path, content.data(), content.size()) ==
         static_cast<int>(content.size());
}

GURL ReadUrlFromGDocFile(const base::FilePath& file_path) {
  return GURL(ReadStringFromGDocFile(file_path, "url"));
}

std::string ReadResourceIdFromGDocFile(const base::FilePath& file_path) {
  return ReadStringFromGDocFile(file_path, "resource_id");
}

}  // namespace util
}  // namespace drive
