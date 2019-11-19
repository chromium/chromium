// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_model_update_listener.h"

#include "base/files/file_enumerator.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "build/build_config.h"

namespace {
// Helper function which finds the model and return its filename from the model
// directory.
std::string GetModelFilenameFromDirectory(const base::FilePath& model_dir) {
  // The model file name always ends with "_index.bin"
  base::FileEnumerator model_enum(model_dir, false, base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("*_index.bin"));

  base::FilePath model_file_path = model_enum.Next();
  std::string model_filename;

  if (!model_file_path.empty()) {
#if defined(OS_WIN)
    model_filename = base::WideToUTF8(model_file_path.value());
#else
    model_filename = model_file_path.value();
#endif  // defined(OS_WIN)
  }

  return model_filename;
}

}  // namespace

// static
OnDeviceModelUpdateListener* OnDeviceModelUpdateListener::GetInstance() {
  static base::NoDestructor<OnDeviceModelUpdateListener> listener;
  return listener.get();
}

OnDeviceModelUpdateListener::OnDeviceModelUpdateListener()
    : task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()})) {}

OnDeviceModelUpdateListener::~OnDeviceModelUpdateListener() = default;

std::unique_ptr<OnDeviceModelUpdateListener::UpdateSubscription>
OnDeviceModelUpdateListener::AddModelUpdateCallback(
    ModelUpdateCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!model_filename_.empty())
    callback.Run(model_filename_);
  return model_update_callbacks_.Add(callback);
}

void OnDeviceModelUpdateListener::OnModelUpdate(
    const base::FilePath& model_dir) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!model_dir.empty() && model_dir != model_dir_) {
    model_dir_ = model_dir;
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&GetModelFilenameFromDirectory, model_dir),
        base::BindOnce([](const std::string filename) {
          if (!filename.empty()) {
            GetInstance()->model_filename_ = filename;
            GetInstance()->model_update_callbacks_.Notify(filename);
          }
        }));
  }
}

void OnDeviceModelUpdateListener::ResetListenerForTest() {
  model_dir_.clear();
  model_filename_.clear();
}
