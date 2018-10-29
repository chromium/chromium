// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/crash_testing_utils.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/base/serializers.h"
#include "chromecast/crash/linux/dump_info.h"

#define RCHECK(cond, retval, err) \
  do {                            \
    if (!(cond)) {                \
      LOG(ERROR) << (err);        \
      return (retval);            \
    }                             \
  } while (0)

namespace chromecast {
namespace {

const char kRatelimitKey[] = "ratelimit";
const char kRatelimitPeriodStartKey[] = "period_start";
const char kRatelimitPeriodDumpsKey[] = "period_dumps";

std::unique_ptr<base::ListValue> ParseLockFile(const std::string& path) {
  std::string lockfile_string;
  RCHECK(base::ReadFileToString(base::FilePath(path), &lockfile_string),
         nullptr,
         "Failed to read file");

  std::vector<std::string> lines = base::SplitString(
      lockfile_string, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::unique_ptr<base::ListValue> dumps = std::make_unique<base::ListValue>();

  // Validate dumps
  for (const std::string& line : lines) {
    if (line.size() == 0)
      continue;
    std::unique_ptr<base::Value> dump_info = DeserializeFromJson(line);
    DumpInfo info(dump_info.get());
    RCHECK(info.valid(), nullptr, "Invalid DumpInfo");
    dumps->Append(std::move(dump_info));
  }

  return dumps;
}

std::unique_ptr<base::Value> ParseMetadataFile(const std::string& path) {
  return DeserializeJsonFromFile(base::FilePath(path));
}

int WriteLockFile(const std::string& path, base::ListValue* contents) {
  DCHECK(contents);
  std::string lockfile;

  for (const auto& elem : *contents) {
    base::Optional<std::string> dump_info = SerializeToJson(elem);
    RCHECK(dump_info, -1, "Failed to serialize DumpInfo");
    lockfile += *dump_info;
    lockfile += "\n";  // Add line seperatators
  }

  return WriteFile(base::FilePath(path), lockfile.c_str(), lockfile.size()) >= 0
             ? 0
             : -1;
}

bool WriteMetadataFile(const std::string& path, const base::Value* metadata) {
  DCHECK(metadata);
  return SerializeJsonToFile(base::FilePath(path), *metadata);
}

}  // namespace

std::unique_ptr<DumpInfo> CreateDumpInfo(const std::string& json_string) {
  std::unique_ptr<base::Value> value(DeserializeFromJson(json_string));
  return std::make_unique<DumpInfo>(value.get());
}

bool FetchDumps(const std::string& lockfile_path,
                std::vector<std::unique_ptr<DumpInfo>>* dumps) {
  DCHECK(dumps);
  std::unique_ptr<base::ListValue> dump_list = ParseLockFile(lockfile_path);
  RCHECK(dump_list, false, "Failed to parse lockfile");

  dumps->clear();

  for (const auto& elem : *dump_list) {
    std::unique_ptr<DumpInfo> dump(new DumpInfo(&elem));
    RCHECK(dump->valid(), false, "Invalid DumpInfo");
    dumps->push_back(std::move(dump));
  }

  return true;
}

bool ClearDumps(const std::string& lockfile_path) {
  std::unique_ptr<base::ListValue> dump_list =
      std::make_unique<base::ListValue>();
  return WriteLockFile(lockfile_path, dump_list.get()) == 0;
}

bool CreateFiles(const std::string& lockfile_path,
                 const std::string& metadata_path) {
  std::unique_ptr<base::DictionaryValue> metadata =
      std::make_unique<base::DictionaryValue>();

  auto ratelimit_fields = std::make_unique<base::DictionaryValue>();
  ratelimit_fields->SetDouble(kRatelimitPeriodStartKey, 0.0);
  ratelimit_fields->SetInteger(kRatelimitPeriodDumpsKey, 0);
  metadata->Set(kRatelimitKey, std::move(ratelimit_fields));

  std::unique_ptr<base::ListValue> dumps = std::make_unique<base::ListValue>();

  return WriteLockFile(lockfile_path, dumps.get()) == 0 &&
         WriteMetadataFile(metadata_path, metadata.get());
}

bool AppendLockFile(const std::string& lockfile_path,
                    const std::string& metadata_path,
                    const DumpInfo& dump) {
  std::unique_ptr<base::ListValue> contents = ParseLockFile(lockfile_path);
  if (!contents) {
    CreateFiles(lockfile_path, metadata_path);
    if (!(contents = ParseLockFile(lockfile_path))) {
      return false;
    }
  }

  contents->Append(dump.GetAsValue());

  return WriteLockFile(lockfile_path, contents.get()) == 0;
}

bool SetRatelimitPeriodStart(const std::string& metadata_path,
                             const base::Time& start) {
  std::unique_ptr<base::Value> contents = ParseMetadataFile(metadata_path);

  base::DictionaryValue* dict;
  base::DictionaryValue* ratelimit_params;
  if (!contents || !contents->GetAsDictionary(&dict) ||
      !dict->GetDictionary(kRatelimitKey, &ratelimit_params)) {
    return false;
  }

  ratelimit_params->SetDouble(kRatelimitPeriodStartKey, start.ToDoubleT());
  return WriteMetadataFile(metadata_path, contents.get()) == 0;
}

}  // namespace chromecast
