// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/screen_ai_service_router.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/thread_pool.h"
#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/service_process_host.h"

namespace {

// TODO(https://crbug.com/1278249): Move file names into a shared constants
// file before adding more files.
class ComponentModelFiles {
 public:
  explicit ComponentModelFiles(const base::FilePath& library_binary_path);
  ComponentModelFiles(const ComponentModelFiles&) = delete;
  ComponentModelFiles& operator=(const ComponentModelFiles&) = delete;
  ~ComponentModelFiles() = default;

  static std::unique_ptr<ComponentModelFiles> LoadComponentFiles();

  base::FilePath library_binary_path_;
  base::File screen2x_model_config_;
  base::File screen2x_model_;
};

ComponentModelFiles::ComponentModelFiles(
    const base::FilePath& library_binary_path)
    : library_binary_path_(library_binary_path),
      screen2x_model_config_(library_binary_path.DirName().Append(
                                 FILE_PATH_LITERAL("screen2x_config.pbtxt")),
                             base::File::FLAG_OPEN | base::File::FLAG_READ),
      screen2x_model_(library_binary_path.DirName().Append(
                          FILE_PATH_LITERAL("screen2x_model.tflite")),
                      base::File::FLAG_OPEN | base::File::FLAG_READ) {}

std::unique_ptr<ComponentModelFiles> ComponentModelFiles::LoadComponentFiles() {
  return std::make_unique<ComponentModelFiles>(
      screen_ai::ScreenAIInstallState::GetInstance()
          ->get_component_binary_path());
}

}  // namespace

namespace screen_ai {

ScreenAIServiceRouter::ScreenAIServiceRouter() = default;
ScreenAIServiceRouter::~ScreenAIServiceRouter() = default;

void ScreenAIServiceRouter::BindScreenAIAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver) {
  LaunchIfNotRunning();

  if (screen_ai_service_.is_bound())
    screen_ai_service_->BindAnnotator(std::move(receiver));
}

void ScreenAIServiceRouter::BindScreenAIAnnotatorClient(
    mojo::PendingRemote<mojom::ScreenAIAnnotatorClient> remote) {
  LaunchIfNotRunning();

  if (screen_ai_service_.is_bound())
    screen_ai_service_->BindAnnotatorClient(std::move(remote));
}

void ScreenAIServiceRouter::BindMainContentExtractor(
    mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver) {
  LaunchIfNotRunning();

  if (screen_ai_service_.is_bound())
    screen_ai_service_->BindMainContentExtractor(std::move(receiver));
}

void ScreenAIServiceRouter::LaunchIfNotRunning() {
  if (screen_ai_service_.is_bound())
    return;

  if (!ScreenAIInstallState::GetInstance()->IsComponentReady()) {
    VLOG(0)
        << "ScreenAI service launch triggered before the component is ready.";
    return;
  }

  // TODO(https://crbug.com/1278249): Make sure the library is sandboxed and
  // loaded from the same folder and component updater doesn't download a new
  // version during sandbox creation.
  content::ServiceProcessHost::Launch(
      screen_ai_service_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Screen AI Service")
          .Pass());
  DCHECK(screen_ai_service_.is_bound());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ComponentModelFiles::LoadComponentFiles),
      base::BindOnce(
          [](base::WeakPtr<ScreenAIServiceRouter> service_router,
             std::unique_ptr<ComponentModelFiles> model_files) {
            if (!service_router)
              return;
            service_router->screen_ai_service_->LoadLibrary(
                std::move(model_files->screen2x_model_config_),
                std::move(model_files->screen2x_model_),
                model_files->library_binary_path_);
          },
          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace screen_ai
