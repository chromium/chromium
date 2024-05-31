// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EARLY_PREFS_EARLY_PREFS_READER_H_
#define CHROMEOS_ASH_COMPONENTS_EARLY_PREFS_EARLY_PREFS_READER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_EARLY_PREFS) EarlyPrefsReader {
 public:
  EarlyPrefsReader(const base::FilePath& data_dir,
                   scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  ~EarlyPrefsReader();

  using ResultCallback = base::OnceCallback<void(bool success)>;

  void ReadFile(ResultCallback result_callback);

  bool HasPref(const std::string& key) const;
  bool IsManaged(const std::string& key) const;
  bool IsRecommended(const std::string& key) const;
  const base::Value* GetValue(const std::string& key) const;

 private:
  void OnFileRead(ResultCallback callback, std::unique_ptr<base::Value> root);
  bool ValidateData(const base::Value::Dict* root) const;
  bool ValidatePref(const base::Value& pref) const;

  std::unique_ptr<base::Value> root_;
  raw_ptr<base::Value::Dict> data_ = nullptr;
  base::FilePath data_file_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::WeakPtrFactory<EarlyPrefsReader> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_EARLY_PREFS_EARLY_PREFS_READER_H_
