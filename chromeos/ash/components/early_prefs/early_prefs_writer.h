// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EARLY_PREFS_EARLY_PREFS_WRITER_H_
#define CHROMEOS_ASH_COMPONENTS_EARLY_PREFS_EARLY_PREFS_WRITER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_EARLY_PREFS) EarlyPrefsWriter
    : private base::ImportantFileWriter::DataSerializer {
 public:
  EarlyPrefsWriter(const base::FilePath& data_dir,
                   scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  ~EarlyPrefsWriter() override;
  void ResetPref(const std::string& key);
  void StoreUserPref(const std::string& key, const base::Value& value);
  void StorePolicy(const std::string& key,
                   const base::Value& value,
                   bool is_recommended);
  void CommitPendingWrites();

 private:
  void ScheduleWrite();
  void SerializeUserPref(const base::Value& value,
                         base::Value::Dict& result) const;
  void SerializePolicy(const base::Value& value,
                       bool is_recommended,
                       base::Value::Dict& result) const;

  std::optional<std::string> SerializeData() override;

  base::Value::Dict root_;
  raw_ptr<base::Value::Dict> data_;
  base::FilePath data_file_;
  std::unique_ptr<base::ImportantFileWriter> writer_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_EARLY_PREFS_EARLY_PREFS_WRITER_H_
