// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_model_update_listener.h"

#include "base/files/file_enumerator.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
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
#if BUILDFLAG(IS_WIN)
    model_filename = base::WideToUTF8(model_file_path.value());
#else
    model_filename = model_file_path.value();
#endif  // BUILDFLAG(IS_WIN)
  }

  return model_filename;
}

}  // namespace

// static
OnDeviceModelUpdateListener* OnDeviceModelUpdateListener::GetInstance() {
  static base::NoDestructor<OnDeviceModelUpdateListener> listener;
  return listener.get();
}

std::string OnDeviceModelUpdateListener::model_filename() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return model_filename_;
}

OnDeviceModelUpdateListener::OnDeviceModelUpdateListener() = default;

OnDeviceModelUpdateListener::~OnDeviceModelUpdateListener() = default;

void OnDeviceModelUpdateListener::OnModelUpdate(
    const base::FilePath& model_dir) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!model_dir.empty() && model_dir != model_dir_) {
    model_dir_ = model_dir;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()},
        base::BindOnce(&GetModelFilenameFromDirectory, model_dir),
        base::BindOnce([](const std::string filename) {
          if (!filename.empty())
            GetInstance()->model_filename_ = filename;
        }));
  }
}

void OnDeviceModelUpdateListener::ResetListenerForTest() {
  model_dir_.clear();
  model_filename_.clear();
}
