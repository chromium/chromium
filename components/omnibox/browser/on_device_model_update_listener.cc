// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_model_update_listener.h"

#include "base/files/file_enumerator.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_util.h"

namespace {
// Helper function which finds the model and return its filename from the model
// directory.
std::string GetModelFilenameFromDirectory(const base::FilePath& model_dir) {
  // The model file name always ends with "_index.bin"
  base::FileEnumerator model_enum(model_dir, false, base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("*_index.bin"));

  base::FilePath model_file_path = model_enum.Next();
  return optimization_guide::FilePathToString(model_file_path);
}

}  // namespace

// static
OnDeviceModelUpdateListener* OnDeviceModelUpdateListener::GetInstance() {
  static base::NoDestructor<OnDeviceModelUpdateListener> listener;
  return listener.get();
}

std::string OnDeviceModelUpdateListener::head_model_filename() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return head_model_filename_;
}

base::FilePath OnDeviceModelUpdateListener::tail_model_filepath() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return tail_model_filepath_;
}

base::FilePath OnDeviceModelUpdateListener::vocab_filepath() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return vocab_filepath_;
}

OnDeviceModelUpdateListener::OnDeviceModelUpdateListener() = default;

OnDeviceModelUpdateListener::~OnDeviceModelUpdateListener() = default;

void OnDeviceModelUpdateListener::OnHeadModelUpdate(
    const base::FilePath& model_dir) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!model_dir.empty() && model_dir != head_model_dir_) {
    head_model_dir_ = model_dir;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()},
        base::BindOnce(&GetModelFilenameFromDirectory, model_dir),
        base::BindOnce([](const std::string filename) {
          if (!filename.empty())
            GetInstance()->head_model_filename_ = filename;
        }));
  }
}

void OnDeviceModelUpdateListener::OnTailModelUpdate(
    const base::FilePath& model_file,
    const base::flat_set<base::FilePath>& additional_files) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!(model_file.empty() || additional_files.empty())) {
    tail_model_filepath_ = model_file;
    for (const auto& file_path : additional_files) {
      if (!file_path.empty()) {
        // Currently only one additional file (i.e. vocabulary) will be sent.
        vocab_filepath_ = file_path;
        break;
      }
    }
  }
}

void OnDeviceModelUpdateListener::ResetListenerForTest() {
  head_model_dir_.clear();
  head_model_filename_.clear();
  tail_model_filepath_.clear();
  vocab_filepath_.clear();
}
