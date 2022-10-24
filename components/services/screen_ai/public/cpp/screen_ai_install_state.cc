// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace screen_ai {

// TODO(https://crbug.com/1278249): Move file names into a shared constants
// file before adding more files.
ComponentModelFiles::ComponentModelFiles(const base::FilePath& library_folder)
    : screen2x_model_config_(
          library_folder.Append(FILE_PATH_LITERAL("screen2x_config.pbtxt")),
          base::File::FLAG_OPEN | base::File::FLAG_READ),
      screen2x_model_(
          library_folder.Append(FILE_PATH_LITERAL("screen2x_model.tflite")),
          base::File::FLAG_OPEN | base::File::FLAG_READ) {}

// static
ScreenAIInstallState* ScreenAIInstallState::GetInstance() {
  static base::NoDestructor<ScreenAIInstallState> instance;
  return instance.get();
}

ScreenAIInstallState::ScreenAIInstallState() = default;
ScreenAIInstallState::~ScreenAIInstallState() = default;

void ScreenAIInstallState::AddObserver(
    ScreenAIInstallState::Observer* observer) {
  observers_.push_back(observer);
  if (is_component_ready())
    observer->ComponentReady();
}

void ScreenAIInstallState::RemoveObserver(
    ScreenAIInstallState::Observer* observer) {
  auto pos = base::ranges::find(observers_, observer);
  if (pos != observers_.end())
    observers_.erase(pos);
}

void ScreenAIInstallState::ComponentFolderVerified(
    const base::FilePath& component_folder) {
  component_binary_path_ =
      component_folder.Append(GetComponentBinaryFileName());

  // Posting task with 'this' is safe here as there is only one object of
  // ScreenAIInstallState and it is only destroyed with browser shutdown.
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&ScreenAIInstallState::OpenComponentFiles,
                                base::Unretained(this)));
}

void ScreenAIInstallState::OpenComponentFiles() {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // TODO(https://crbug.com/1278249): This approach fails if more than one user
  // profile use the service because the file handles are destroyed after the
  // first service initialization. We need to have the handles per profile
  // (e.g. in |ScreenAIServiceRouter|) or make sure the opened files are not
  // get closed after first use.
  std::unique_ptr<ComponentModelFiles> model_files =
      std::make_unique<ComponentModelFiles>(component_binary_path_.DirName());

  // Posting task with 'this' is safe here as there is only one object of
  // ScreenAIInstallState and it is only destroyed with browser shutdown.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ScreenAIInstallState::SetComponentModelFiles,
                     base::Unretained(this), std::move(model_files)));
}

void ScreenAIInstallState::SetComponentModelFiles(
    std::unique_ptr<ComponentModelFiles> model_files) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!component_model_files_);
  component_model_files_ = std::move(model_files);

  SetComponentReady();
}

ComponentModelFiles* ScreenAIInstallState::GetComponentModelFiles() {
  return component_model_files_.get();
}

void ScreenAIInstallState::SetComponentReady() {
  component_ready_ = true;

  for (ScreenAIInstallState::Observer* observer : observers_)
    observer->ComponentReady();
}

}  // namespace screen_ai
