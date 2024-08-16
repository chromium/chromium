// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/early_prefs/early_prefs_writer.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/early_prefs/early_prefs_constants.h"

namespace ash {
namespace {

constexpr int kCurrentSchemaVersion = 1;

}  // namespace

EarlyPrefsWriter::EarlyPrefsWriter(
    const base::FilePath& data_dir,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : file_task_runner_(std::move(file_task_runner)) {
  data_file_ = data_dir.Append(early_prefs::kEarlyPrefsFileName);
  writer_ = std::make_unique<base::ImportantFileWriter>(
      data_file_, file_task_runner_, early_prefs::kEarlyPrefsHistogramName);

  root_.Set(early_prefs::kSchemaKey, kCurrentSchemaVersion);
  data_ = root_.EnsureDict(early_prefs::kDataKey);

  // Overwrite existing file, to clear previous prefs, if any.
  ScheduleWrite();
}

EarlyPrefsWriter::~EarlyPrefsWriter() = default;

void EarlyPrefsWriter::StoreUserPref(const std::string& key,
                                     const base::Value& value) {
  // Check if value is the same.
  auto* existing = data_->FindDict(key);
  base::Value::Dict new_value;
  SerializeUserPref(value, new_value);
  if (existing) {
    if (new_value == *existing) {
      // Values are same, no need to store data
      return;
    }
  }
  data_->Set(key, std::move(new_value));
  ScheduleWrite();
}

void EarlyPrefsWriter::StorePolicy(const std::string& key,
                                   const base::Value& value,
                                   bool is_recommended) {
  // Check if value is the same.
  auto* existing = data_->FindDict(key);
  base::Value::Dict new_value;
  SerializePolicy(value, is_recommended, new_value);
  if (existing) {
    if (new_value == *existing) {
      // Values are same, no need to store data
      return;
    }
  }
  data_->Set(key, std::move(new_value));
  ScheduleWrite();
}

void EarlyPrefsWriter::SerializeUserPref(const base::Value& value,
                                         base::Value::Dict& result) const {
  result.Set(early_prefs::kPrefIsManagedKey, false);
  result.Set(early_prefs::kPrefValueKey, value.Clone());
}

void EarlyPrefsWriter::SerializePolicy(const base::Value& value,
                                       bool is_recommended,
                                       base::Value::Dict& result) const {
  result.Set(early_prefs::kPrefIsManagedKey, true);
  result.Set(early_prefs::kPrefIsRecommendedKey, is_recommended);
  result.Set(early_prefs::kPrefValueKey, value.Clone());
}

void EarlyPrefsWriter::ResetPref(const std::string& key) {
  bool changed = data_->Remove(key);
  if (changed) {
    ScheduleWrite();
  }
}

void EarlyPrefsWriter::CommitPendingWrites() {
  if (writer_->HasPendingWrite()) {
    writer_->DoScheduledWrite();
  }
}

void EarlyPrefsWriter::ScheduleWrite() {
  writer_->ScheduleWrite(this);
}

std::optional<std::string> EarlyPrefsWriter::SerializeData() {
  std::string output;
  if (!base::JSONWriter::Write(root_, &output)) {
    NOTREACHED() << "Failed to serialize early preferences : " << data_file_;
  }
  return output;
}

}  // namespace ash
