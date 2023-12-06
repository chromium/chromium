// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/early_prefs/early_prefs_reader.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chromeos/ash/components/early_prefs/early_prefs_constants.h"

namespace ash {
namespace {

constexpr int kCurrentSchemaVersion = 1;

std::unique_ptr<base::Value> ReadFileFromDisk(const base::FilePath& data_file) {
  std::unique_ptr<base::Value> result;

  int error_code;
  std::string error_msg;
  JSONFileValueDeserializer deserializer(data_file);
  result = deserializer.Deserialize(&error_code, &error_msg);
  if (!result) {
    if (error_code == JSONFileValueDeserializer::JSON_NO_SUCH_FILE) {
      // Somewhat expected situation, do not log.
      return nullptr;
    }
    LOG(ERROR) << "Error while loading early prefs file: " << error_msg;
  }
  return result;
}

}  // namespace

EarlyPrefsReader::EarlyPrefsReader(
    const base::FilePath& data_dir,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : file_task_runner_(std::move(file_task_runner)) {
  data_file_ = data_dir.Append(early_prefs::kEarlyPrefsFileName);
}

EarlyPrefsReader::~EarlyPrefsReader() = default;

void EarlyPrefsReader::ReadFile(ResultCallback result_callback) {
  CHECK(!data_);
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadFileFromDisk, data_file_),
      base::BindOnce(&EarlyPrefsReader::OnFileRead, weak_factory_.GetWeakPtr(),
                     std::move(result_callback)));
}

void EarlyPrefsReader::OnFileRead(ResultCallback callback,
                                  std::unique_ptr<base::Value> root) {
  if (!root) {
    std::move(callback).Run(false);
    return;
  }
  if (!ValidateData(root->GetIfDict())) {
    std::move(callback).Run(false);
    return;
  }
  root_ = std::move(root);
  data_ = root_->GetDict().FindDict(early_prefs::kDataKey);
  CHECK(data_);
  std::move(callback).Run(true);
}

bool EarlyPrefsReader::ValidateData(const base::Value::Dict* root) const {
  if (!root) {
    return false;
  }
  if (root->size() != 2) {
    return false;
  }
  auto schema = root->FindInt(early_prefs::kSchemaKey);
  if (!schema || schema.value() != kCurrentSchemaVersion) {
    return false;
  }
  auto* data = root->FindDict(early_prefs::kDataKey);
  if (!data) {
    return false;
  }
  for (auto pref : *data) {
    if (!ValidatePref(pref.second)) {
      return false;
    }
  }
  return true;
}

bool EarlyPrefsReader::ValidatePref(const base::Value& pref) const {
  if (!pref.is_dict()) {
    return false;
  }
  const auto& dict = pref.GetDict();
  auto is_managed = dict.FindBool(early_prefs::kPrefIsManagedKey);
  if (!is_managed) {
    return false;
  }
  if (is_managed.value()) {
    auto is_recommended = dict.FindBool(early_prefs::kPrefIsRecommendedKey);
    if (!is_recommended) {
      return false;
    }
  }
  if (!dict.Find(early_prefs::kPrefValueKey)) {
    return false;
  }
  return true;
}

bool EarlyPrefsReader::HasPref(const std::string& key) const {
  if (!data_) {
    return false;
  }
  return data_->Find(key) != nullptr;
}

bool EarlyPrefsReader::IsManaged(const std::string& key) const {
  CHECK(HasPref(key));
  return data_->FindDict(key)->FindBool(early_prefs::kPrefIsManagedKey).value();
}

bool EarlyPrefsReader::IsRecommended(const std::string& key) const {
  CHECK(IsManaged(key));
  return data_->FindDict(key)
      ->FindBool(early_prefs::kPrefIsRecommendedKey)
      .value();
}

const base::Value* EarlyPrefsReader::GetValue(const std::string& key) const {
  CHECK(HasPref(key));
  return data_->FindDict(key)->Find(early_prefs::kPrefValueKey);
}

}  // namespace ash
