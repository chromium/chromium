// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/config_reader.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace fuchsia_component_support {

namespace {

bool HaveConflicts(const base::Value::Dict& dict1,
                   const base::Value::Dict& dict2) {
  for (auto item : dict1) {
    const base::Value* value = dict2.Find(item.first);
    if (!value)
      continue;
    if (!value->is_dict())
      return true;
    if (HaveConflicts(item.second.GetDict(), value->GetDict()))
      return true;
  }

  return false;
}

base::Value::Dict ReadConfigFile(const base::FilePath& path) {
  std::string file_content;
  bool loaded = base::ReadFileToString(path, &file_content);
  CHECK(loaded) << "Couldn't read config file: " << path;

  auto parsed = base::JSONReader::ReadAndReturnValueWithError(file_content);
  CHECK(parsed.has_value())
      << "Failed to parse " << path << ": " << parsed.error().message;
  CHECK(parsed->is_dict()) << "Config is not a JSON dictionary: " << path;

  return std::move(parsed->GetDict());
}

std::optional<base::Value::Dict> ReadConfigsFromDir(const base::FilePath& dir) {
  base::FileEnumerator configs(dir, false, base::FileEnumerator::FILES,
                               "*.json");
  std::optional<base::Value::Dict> config;
  for (base::FilePath path; !(path = configs.Next()).empty();) {
    base::Value::Dict path_config = ReadConfigFile(path);
    if (config) {
      CHECK(!HaveConflicts(*config, path_config));
      config->Merge(std::move(path_config));
    } else {
      config = std::move(path_config);
    }
  }

  return config;
}

}  // namespace

const std::optional<base::Value::Dict>& LoadPackageConfig() {
  // Package configurations do not change at run-time, so read the configuration
  // on the first call and cache the result.
  static base::NoDestructor<std::optional<base::Value::Dict>> config(
      ReadConfigsFromDir(base::FilePath("/config/data")));

  return *config;
}

std::optional<base::Value::Dict> LoadConfigFromDirForTest(  // IN-TEST
    const base::FilePath& dir) {
  return ReadConfigsFromDir(dir);
}

}  // namespace fuchsia_component_support
