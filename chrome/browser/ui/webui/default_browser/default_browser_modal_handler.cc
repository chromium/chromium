// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/default_browser_modal_handler.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_surface_manager.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal_ui.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/installer/util/shell_util.h"
#endif

DefaultBrowserModalHandler::DefaultBrowserModalHandler(
    content::WebUI* web_ui,
    mojo::PendingRemote<default_browser_modal::mojom::Page> page,
    mojo::PendingReceiver<default_browser_modal::mojom::PageHandler> receiver)
    : web_ui_(web_ui),
      page_(std::move(page)),
      receiver_(this, std::move(receiver)) {
  if (!default_browser::IsDefaultBrowserModalSticky()) {
    return;
  }

  auto* prompt_manager = DefaultBrowserPromptManager::GetInstance();
  auto* surface_manager = prompt_manager->GetPromptSurfaceManager();
  if (!surface_manager) {
    return;
  }

  has_accepted_subscription_ = surface_manager->RegisterHasAcceptedChanged(
      base::BindRepeating(&DefaultBrowserModalHandler::OnHasAcceptedChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  if (surface_manager->has_accepted()) {
    OnHasAcceptedChanged(true);
  }
}

DefaultBrowserModalHandler::~DefaultBrowserModalHandler() = default;

void DefaultBrowserModalHandler::Cancel() {
  auto* prompt_manager = DefaultBrowserPromptManager::GetInstance();
  if (auto* surface_manager = prompt_manager->GetPromptSurfaceManager()) {
    surface_manager->HandleDismiss();
    prompt_manager->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kDismiss);
  }

  if (web_ui_ && web_ui_->GetWebContents()) {
    web_ui_->GetWebContents()->Close();
  }
}

void DefaultBrowserModalHandler::Confirm() {
  auto* prompt_manager = DefaultBrowserPromptManager::GetInstance();
  auto* surface_manager = prompt_manager->GetPromptSurfaceManager();
  if (!surface_manager) {
    return;
  }

  surface_manager->HandleAccept();

  if (default_browser::IsDefaultBrowserModalSticky()) {
    return;
  }

  prompt_manager->CloseAllPrompts(
      DefaultBrowserPromptManager::CloseReason::kAccept);

  if (web_ui_ && web_ui_->GetWebContents()) {
    web_ui_->GetWebContents()->Close();
  }
}

void DefaultBrowserModalHandler::TryAgain() {
  if (auto* surface_manager = DefaultBrowserPromptManager::GetInstance()
                                  ->GetPromptSurfaceManager()) {
    surface_manager->HandleRetry();
  }
#if BUILDFLAG(IS_WIN)
  // Re-open Windows Settings directly rather than via `DefaultBrowserSetter`:
  // `OpenSystemSettingsHelper::Begin()` cancels any in-flight watcher,
  // which would drop the first attempt's metrics callback.
  base::ThreadPool::CreateCOMSTATaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE})
      ->PostTask(FROM_HERE, base::BindOnce([]() {
                   base::FilePath chrome_exe;
                   if (base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
                     ShellUtil::ShowMakeChromeDefaultSystemUI(chrome_exe);
                   }
                 }));
#endif  // BUILDFLAG(IS_WIN)
}

void DefaultBrowserModalHandler::CheckDefaultStatusAndMaybeClose(
    CheckDefaultStatusAndMaybeCloseCallback callback) {
  if (auto* manager =
          default_browser::DefaultBrowserManager::From(g_browser_process)) {
    manager->GetDefaultBrowserState(
        base::BindOnce(&DefaultBrowserModalHandler::OnCheckDefaultStatusResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(false);
  }
}

void DefaultBrowserModalHandler::OnCheckDefaultStatusResult(
    CheckDefaultStatusAndMaybeCloseCallback callback,
    shell_integration::DefaultWebClientState state) {
  const bool is_default =
      (state == shell_integration::DefaultWebClientState::IS_DEFAULT);

  std::move(callback).Run(is_default);

  if (is_default) {
    DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
        DefaultBrowserPromptManager::CloseReason::kAccept);
  }
}

void DefaultBrowserModalHandler::OnHasAcceptedChanged(bool has_accepted) {
  if (page_) {
    page_->OnHasAcceptedChanged(has_accepted);
  }
}

void DefaultBrowserModalHandler::ShowUI() {
  if (!web_ui_ || !web_ui_->GetController()) {
    return;
  }

  auto* top_chrome_controller =
      web_ui_->GetController()->GetAs<DefaultBrowserModalUI>();
  if (!top_chrome_controller) {
    return;
  }

  if (top_chrome_controller->embedder()) {
    top_chrome_controller->embedder()->ShowUI();
  }
}
