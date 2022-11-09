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

// Helper function which returns tail model filename and its vocabulary.
std::pair<std::string, std::string> GetTailModelAndVocabFilenames(
    const base::FilePath& model_file_path,
    const base::flat_set<base::FilePath>& additional_files) {
  std::string model_filename, vocab_filename;

  if (!model_file_path.empty())
    model_filename = optimization_guide::FilePathToString(model_file_path);

  if (additional_files.size() > 0)
    vocab_filename =
        optimization_guide::FilePathToString(*additional_files.begin());

  return std::make_pair(model_filename, vocab_filename);
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

std::string OnDeviceModelUpdateListener::tail_model_filename() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return tail_model_filename_;
}

std::string OnDeviceModelUpdateListener::vocab_filename() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return vocab_filename_;
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
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()},
        base::BindOnce(&GetTailModelAndVocabFilenames, model_file,
                       additional_files),
        base::BindOnce(
            [](const std::pair<std::string, std::string>& filenames) {
              if (!filenames.first.empty())
                GetInstance()->tail_model_filename_ = filenames.first;
              if (!filenames.second.empty())
                GetInstance()->vocab_filename_ = filenames.second;
            }));
  }
}

void OnDeviceModelUpdateListener::ResetListenerForTest() {
  head_model_dir_.clear();
  head_model_filename_.clear();
  tail_model_filename_.clear();
  vocab_filename_.clear();
}
