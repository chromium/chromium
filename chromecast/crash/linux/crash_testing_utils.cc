// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/crash_testing_utils.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chromecast/base/path_utils.h"
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

std::optional<base::Value::List> ParseLockFile(const std::string& path) {
  std::string lockfile_string;
  RCHECK(base::ReadFileToString(base::FilePath(path), &lockfile_string),
         std::nullopt, "Failed to read file");

  std::vector<std::string> lines = base::SplitString(
      lockfile_string, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  base::Value::List dumps;

  // Validate dumps
  for (const std::string& line : lines) {
    if (line.size() == 0)
      continue;
    std::optional<base::Value> dump_info = base::JSONReader::Read(line);
    RCHECK(dump_info.has_value(), std::nullopt, "Invalid DumpInfo");
    DumpInfo info(&dump_info.value());
    RCHECK(info.valid(), std::nullopt, "Invalid DumpInfo");
    dumps.Append(std::move(dump_info.value()));
  }

  return dumps;
}

std::unique_ptr<base::Value> ParseMetadataFile(const std::string& path) {
  base::FilePath file_path(path);
  JSONFileValueDeserializer deserializer(file_path);
  int error_code = -1;
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  DLOG_IF(ERROR, !value) << "JSON error " << error_code << ":" << error_msg;
  return value;
}

int WriteLockFile(const std::string& path, const base::Value::List& contents) {
  std::string lockfile;

  for (const auto& elem : contents) {
    std::string dump_info;
    bool ret = base::JSONWriter::Write(elem, &dump_info);
    RCHECK(ret, -1, "Failed to serialize DumpInfo");
    lockfile += dump_info;
    lockfile += "\n";  // Add line seperatators
  }

  return WriteFile(base::FilePath(path), lockfile) ? 0 : -1;
}

bool WriteMetadataFile(const std::string& path,
                       const base::Value::Dict& metadata) {
  base::FilePath file_path(path);
  JSONFileValueSerializer serializer(file_path);
  return serializer.Serialize(metadata);
}

}  // namespace

std::unique_ptr<DumpInfo> CreateDumpInfo(const std::string& json_string) {
  std::optional<base::Value> value = base::JSONReader::Read(json_string);
  return value.has_value() ? std::make_unique<DumpInfo>(&value.value())
                           : std::make_unique<DumpInfo>(nullptr);
}

bool FetchDumps(const std::string& lockfile_path,
                std::vector<std::unique_ptr<DumpInfo>>* dumps) {
  DCHECK(dumps);
  std::optional<base::Value::List> dump_list = ParseLockFile(lockfile_path);
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
  base::Value::List dump_list;
  return WriteLockFile(lockfile_path, dump_list) == 0;
}

bool CreateFiles(const std::string& lockfile_path,
                 const std::string& metadata_path) {
  base::Value::Dict metadata;

  base::Value::Dict ratelimit_fields;
  ratelimit_fields.Set(kRatelimitPeriodStartKey, 0.0);
  ratelimit_fields.Set(kRatelimitPeriodDumpsKey, 0);
  metadata.Set(kRatelimitKey, std::move(ratelimit_fields));

  base::Value::List dumps;

  return WriteLockFile(lockfile_path, dumps) == 0 &&
         WriteMetadataFile(metadata_path, metadata);
}

bool AppendLockFile(const std::string& lockfile_path,
                    const std::string& metadata_path,
                    const DumpInfo& dump) {
  std::optional<base::Value::List> contents = ParseLockFile(lockfile_path);
  if (!contents) {
    CreateFiles(lockfile_path, metadata_path);
    if (!(contents = ParseLockFile(lockfile_path))) {
      return false;
    }
  }

  contents->Append(dump.GetAsValue());

  return WriteLockFile(lockfile_path, *contents) == 0;
}

bool SetRatelimitPeriodStart(const std::string& metadata_path,
                             const base::Time& start) {
  std::unique_ptr<base::Value> contents = ParseMetadataFile(metadata_path);
  if (!contents || !contents->is_dict())
    return false;

  base::Value::Dict* ratelimit_params =
      contents->GetDict().FindDict(kRatelimitKey);
  if (!ratelimit_params)
    return false;

  ratelimit_params->Set(kRatelimitPeriodStartKey,
                        start.InSecondsFSinceUnixEpoch());
  return WriteMetadataFile(metadata_path, contents->GetDict()) == 0;
}

}  // namespace chromecast
