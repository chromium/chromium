// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler_v2.h"

#include <optional>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
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
      identity_manager_(identity_manager),
      web_contents_(CHECK_DEREF(web_contents)),
      cookie_synchronizer_(std::make_unique<glic::GlicCookieSynchronizer>(
          &profile_.get(),
          identity_manager,
          content::StoragePartitionConfig::Create(
              &profile_.get(),
              chrome::kChromeUISkillsHost,
              /*partition_name=*/"glicskillspart",
              /*in_memory=*/true))),
      delegate_(delegate) {
  if (auto* service =
          SkillsServiceFactory::GetForProfile(base::to_address(profile_))) {
    service_observation_.Observe(service);
  }
}

SkillsPageHandlerV2::~SkillsPageHandlerV2() = default;

void SkillsPageHandlerV2::SetPage(
    mojo::PendingRemote<skills::mojom::SkillsPageV2> page) {
  page_.reset();
  page_.Bind(std::move(page));
}

void SkillsPageHandlerV2::GetProvidedSkill(const std::string& skill_id,
                                           GetProvidedSkillCallback callback) {
  const Skill* skill = nullptr;
  if (auto* service =
          SkillsServiceFactory::GetForProfile(base::to_address(profile_))) {
    skill = service->GetSkillById(skill_id);
  }

  if (!skill) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(*skill);
}

void SkillsPageHandlerV2::GetProvidedSkills(
    GetProvidedSkillsCallback callback) {
  std::vector<Skill> skills;
  if (auto* service =
          SkillsServiceFactory::GetForProfile(base::to_address(profile_))) {
    for (const auto& [id, skill] : service->GetProvidedSkills()) {
      skills.push_back(*skill);
    }
  }
  std::move(callback).Run(std::move(skills));
}

void SkillsPageHandlerV2::OnProvidedSkillsChanged(SkillsProvider* provider) {
  if (!page_.is_bound()) {
    return;
  }
  auto* service =
      SkillsServiceFactory::GetForProfile(base::to_address(profile_));
  if (!service) {
    return;
  }
  std::vector<Skill> skills;
  for (const auto& [id, skill] : service->GetProvidedSkills()) {
    skills.push_back(*skill);
  }
  page_->LoadProvidedSkills(std::move(skills));
}

void SkillsPageHandlerV2::OnSkillUpdated(
    std::string_view skill_id,
    SkillsService::UpdateSource update_source,
    bool is_position_changed) {
  if (page_.is_bound()) {
    page_->OnUserSkillsUpdated();
  }
}

void SkillsPageHandlerV2::SyncCookies(SyncCookiesCallback callback) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSkillsV2Origin)) {
    std::move(callback).Run(true);
    return;
  }
  cookie_synchronizer_->CopyCookiesToWebviewStoragePartition(
      std::move(callback));
}

void SkillsPageHandlerV2::ShowSaveToast() {
  if (delegate_) {
    delegate_->OnSkillSaved("");
    return;
  }
  if (auto* controller = GetHostTabController()) {
    controller->OnSkillSaved("");
  }
}

void SkillsPageHandlerV2::ShowSaveAndInvokeToast(
    const std::string& skill_id,
    const std::string& skill_name,
    const std::string& skill_icon) {
  if (delegate_) {
    delegate_->OnSkillSaved(skill_id, skill_name, skill_icon);
    CloseDialog(nullptr);
    return;
  }
  if (auto* controller = GetHostTabController()) {
    controller->OnSkillSaved(skill_id, skill_name, skill_icon);
  }
}

void SkillsPageHandlerV2::ShowDeleteToast(const std::string& skill_id,
                                          ShowDeleteToastCallback callback) {
  auto wrapped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false);
  if (delegate_) {
    delegate_->OnSkillDeleted(skill_id, std::move(wrapped_callback));
    return;
  }
  if (auto* controller = GetHostTabController()) {
    controller->OnSkillDeleted(skill_id, std::move(wrapped_callback));
  }
}

void SkillsPageHandlerV2::InvokeSkill(const std::string& skill_id,
                                      const std::string& skill_name,
                                      const std::string& skill_icon) {
  if (!SkillsServiceFactory::IsSkillsEnabledForProfile(&profile_.get())) {
    return;
  }
  if (delegate_) {
    delegate_->InvokeSkill(skill_id, skill_name, skill_icon,
                           /*auto_submit=*/true);
    return;
  }
  if (auto* controller = GetHostTabController()) {
    controller->InvokeSkill(skill_id, skill_name, skill_icon,
                            /*auto_submit=*/true);
  }
}

void SkillsPageHandlerV2::SendPrompt(const std::string& prompt) {
  if (delegate_) {
    delegate_->SendPrompt(prompt);
    return;
  }
  if (auto* controller = GetHostTabController()) {
    controller->SendPrompt(prompt);
  }
}

void SkillsPageHandlerV2::CloseDialog(
    skills::mojom::PendingEditorDataPtr data) {
  // First store any data in user data, then proceed with dialog and handler
  // destruction.
  if (data) {
    PendingEditorDataHandler::GetOrCreateForProfile(&profile_.get())
        ->StoreData(std::move(data));
    if (delegate_) {
      if (BrowserWindowInterface* browser =
              delegate_->GetBrowserWindowInterface()) {
        if (auto* tab_list = TabListInterface::From(browser)) {
          tab_list->OpenTab(GURL("chrome://skills/editor"), -1,
                            /*foreground=*/true);
#if !BUILDFLAG(IS_ANDROID)
        } else {
          browser->OpenGURL(
              GURL("chrome://skills/editor"),
              ui::mojom::WindowOpenDisposition::kNewForegroundTab);
#endif
        }
      }
    }
  }
  if (delegate_) {
    delegate_->CloseDialog();
  }
}

void SkillsPageHandlerV2::GetPendingEditorData(
    GetPendingEditorDataCallback callback) {
  auto data = PendingEditorDataHandler::GetOrCreateForProfile(&profile_.get())
                  ->RetrieveData();
  // Note: Data is cleared when this callback runs.
  std::move(callback).Run(std::move(data));
}

void SkillsPageHandlerV2::SignIn() {
  if (!identity_manager_) {
    return;
  }

  CoreAccountInfo primary_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

#if !BUILDFLAG(IS_ANDROID)
  if (!primary_account_info.IsEmpty()) {
    signin_ui_util::ShowReauthForAccount(&profile_.get(),
                                         primary_account_info.email,
                                         signin_metrics::AccessPoint::kSkills);
  } else {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    signin_ui_util::SignInFromSingleAccountPromo(
        &profile_.get(), CoreAccountInfo(),
        signin_metrics::AccessPoint::kSkills);
#endif
  }
#endif
}

SkillsUiTabControllerInterface* SkillsPageHandlerV2::GetHostTabController() {
  return SkillsUiTabControllerInterface::From(
      tabs::TabInterface::MaybeGetFromContents(&web_contents_.get()));
}

}  // namespace skills
