// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler_v2.h"

#include "base/check_deref.h"
#include "base/supports_user_data.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/skills/skills_ui_window_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace skills {
namespace {

// A profile-scoped container for storing pending editor data that needs to be
// passed when transitioning between closing the skills dialog and opening the
// skills editor in a new foreground tab. The dialog's handler stores the data
// before closing, and the editor's handler retrieves and clears the data upon
// loading.
class PendingEditorDataHandler : public base::SupportsUserData::Data {
 public:
  static constexpr char kUserDataKey[] = "pending_editor_data_handler";

  static PendingEditorDataHandler* GetOrCreateForProfile(Profile* profile) {
    auto* handler = static_cast<PendingEditorDataHandler*>(
        profile->GetUserData(kUserDataKey));
    if (!handler) {
      auto new_handler = std::make_unique<PendingEditorDataHandler>();
      handler = new_handler.get();
      profile->SetUserData(kUserDataKey, std::move(new_handler));
    }
    return handler;
  }

  void StoreData(mojom::PendingEditorDataPtr data) {
    pending_data_ = std::move(data);
  }

  mojom::PendingEditorDataPtr RetrieveData() {
    return std::move(pending_data_);
  }

 private:
  mojom::PendingEditorDataPtr pending_data_;
};

}  // namespace

SkillsPageHandlerV2::SkillsPageHandlerV2(
    mojo::PendingReceiver<::skills::mojom::SkillsPageHandler> receiver,
    Profile* profile,
    signin::IdentityManager* identity_manager,
    content::WebContents* web_contents,
    base::WeakPtr<SkillsDialogDelegate> delegate)
    : receiver_(this, std::move(receiver)),
      profile_(CHECK_DEREF(profile)),
      web_contents_(CHECK_DEREF(web_contents)),
      cookie_synchronizer_(std::make_unique<glic::GlicCookieSynchronizer>(
          &profile_.get(),
          identity_manager,
          content::StoragePartitionConfig::Create(
              &profile_.get(),
              chrome::kChromeUISkillsHost,
              /*partition_name=*/"glicskillspart",
              /*in_memory=*/true))),
      delegate_(delegate) {}

SkillsPageHandlerV2::~SkillsPageHandlerV2() = default;

void SkillsPageHandlerV2::SyncCookies(SyncCookiesCallback callback) {
  cookie_synchronizer_->CopyCookiesToWebviewStoragePartition(
      std::move(callback));
}

void SkillsPageHandlerV2::ShowSaveToast() {
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          &web_contents_.get());
  if (!browser) {
    return;
  }

  auto* window_controller = SkillsUiWindowController::From(browser);
  if (!window_controller) {
    return;
  }

  window_controller->ShowToast(ToastId::kSkillSavedWithoutInvokeButton);
}

void SkillsPageHandlerV2::ShowDeleteToast(const std::string& skill_id,
                                          ShowDeleteToastCallback callback) {
  auto wrapped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false);
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          &web_contents_.get());
  if (!browser) {
    return;
  }

  auto* window_controller = SkillsUiWindowController::From(browser);
  if (!window_controller) {
    return;
  }
  window_controller->ShowToast(ToastId::kSkillDeleted, skill_id,
                               std::move(wrapped_callback));
}

void SkillsPageHandlerV2::InvokeSkill(const std::string& skill_id,
                                      const std::string& skill_name,
                                      const std::string& skill_icon) {
  tabs::TabInterface* tab =
      tabs::TabInterface::GetFromContents(&web_contents_.get());
  if (!tab) {
    return;
  }
  if (auto* tab_controller = SkillsUiTabControllerInterface::From(tab)) {
    tab_controller->InvokeSkill(skill_id, skill_name, skill_icon);
  }
}

void SkillsPageHandlerV2::SendPrompt(const std::string& prompt) {
  tabs::TabInterface* tab =
      tabs::TabInterface::GetFromContents(&web_contents_.get());
  if (!tab) {
    return;
  }
  if (auto* tab_controller = SkillsUiTabControllerInterface::From(tab)) {
    tab_controller->SendPrompt(prompt);
  }
}

void SkillsPageHandlerV2::CloseDialog(
    skills::mojom::PendingEditorDataPtr data) {
  if (!delegate_) {
    return;
  }
  // First store any data in user data, then proceed with dialog and handler
  // destruction.
  if (data) {
    PendingEditorDataHandler::GetOrCreateForProfile(&profile_.get())
        ->StoreData(std::move(data));
    delegate_->GetBrowserWindowInterface()->OpenGURL(
        GURL("chrome://skills/editor"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB);
  }
  delegate_->CloseDialog();
}

void SkillsPageHandlerV2::GetPendingEditorData(
    GetPendingEditorDataCallback callback) {
  auto data = PendingEditorDataHandler::GetOrCreateForProfile(&profile_.get())
                  ->RetrieveData();
  // Note: Data is cleared when this callback runs.
  std::move(callback).Run(std::move(data));
}

}  // namespace skills
