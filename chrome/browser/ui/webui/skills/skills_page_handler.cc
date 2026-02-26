// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_window_controller.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_metrics.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace skills {
namespace {
using FirstPartySkillsMap =
    base::flat_map</*category=*/std::string, std::vector<skills::Skill>>;

FirstPartySkillsMap Translate1PSkillsMap(
    const SkillsService::SkillsMap& skills_map) {
  FirstPartySkillsMap translated_map;
  for (const auto& [id, skill] : skills_map) {
    skills::Skill translated_skill;
    translated_skill.id = skill.id();
    translated_skill.name = skill.name();
    translated_skill.icon = skill.icon();
    translated_skill.prompt = skill.prompt();
    translated_skill.description = skill.description();
    translated_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
    translated_map[skill.category()].push_back(std::move(translated_skill));
  }
  return translated_map;
}

bool IsServiceReady(const SkillsService* service) {
  bool is_ready = service && service->GetServiceStatus() ==
                                 SkillsService::ServiceStatus::kReady;
  if (!is_ready) {
    RecordSkillsManagementError(SkillsManagementError::kSkillsServiceNotReady);
  }
  return is_ready;
}

}  // namespace

// PendingSave1PRequest struct definitions:
PendingSave1PRequest::PendingSave1PRequest(
    std::string id,
    SkillsPageHandler::MaybeSave1PSkillCallback cb)
    : skill_id(std::move(id)), callback(std::move(cb)) {}
PendingSave1PRequest::~PendingSave1PRequest() = default;
PendingSave1PRequest::PendingSave1PRequest(PendingSave1PRequest&&) = default;
PendingSave1PRequest& PendingSave1PRequest::operator=(PendingSave1PRequest&&) =
    default;

// SkillsPageHandler class definitions:
SkillsPageHandler::SkillsPageHandler(
    mojo::PendingReceiver<skills::mojom::PageHandler> receiver,
    mojo::PendingRemote<skills::mojom::SkillsPage> page,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_contents_(CHECK_DEREF(web_contents)),
      profile_(CHECK_DEREF(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  if (auto* service =
          SkillsServiceFactory::GetForProfile(base::to_address(profile_))) {
    service_observation_.Observe(service);
  }
}

SkillsPageHandler::~SkillsPageHandler() = default;

void SkillsPageHandler::OpenSkillsDialog(
    mojom::SkillsDialogType dialog_type,
    const std::optional<skills::Skill>& skill) {
  // chrome://skills is always expected to open in a tab otherwise
  // GetFromContents will crash.
  // TODO(b/475599531): Pass in dialog type.
  if (auto* tab_controller = SkillsUiTabControllerInterface::From(
          tabs::TabInterface::GetFromContents(&web_contents_.get()))) {
    tab_controller->ShowDialog(skill.value_or(skills::Skill()));
  } else {
    RecordSkillsManagementError(SkillsManagementError::kTabControllerDNE);
  }
}

void SkillsPageHandler::GetInitialUserSkills(
    GetInitialUserSkillsCallback callback) {
  std::vector<skills::Skill> skills;
  auto scoped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::vector<skills::Skill>());
  auto* service =
      SkillsServiceFactory::GetForProfile(base::to_address(profile_));
  if (!IsServiceReady(service)) {
    return;
  }

  for (const auto& skill : service->GetSkills()) {
    skills.push_back(*skill);
  }
  std::move(scoped_callback).Run(std::move(skills));
}

void SkillsPageHandler::DeleteSkill(const std::string& skill_id) {
  auto* service =
      SkillsServiceFactory::GetForProfile(base::to_address(profile_));
  if (!IsServiceReady(service)) {
    return;
  }
  service->DeleteSkill(skill_id, SkillsService::UpdateSource::kLocal);
}

void SkillsPageHandler::OnSkillUpdated(
    std::string_view skill_id,
    SkillsService::UpdateSource update_source,
    bool is_position_changed) {
  if (auto* service =
          SkillsServiceFactory::GetForProfile(base::to_address(profile_))) {
    const auto* skill = service->GetSkillById(skill_id);
    if (skill) {
      // If the skill exists, this means the skill was either added or updated.
      page_->UpdateSkill(*skill);
    } else {
      // If the skill no longer exists, this means the skill was deleted.
      page_->RemoveSkill(std::string(skill_id));

      // Show a toast to the user that the skill was deleted and if the deletion
      // was triggered from the UI.
      auto* tabs = tabs::TabInterface::GetFromContents(&web_contents_.get());
      auto* browser_window_interface = tabs->GetBrowserWindowInterface();
      if (browser_window_interface &&
          update_source == SkillsService::UpdateSource::kLocal) {
        SkillsUiWindowController::From(browser_window_interface)
            ->OnSkillDeleted();
      }
    }
  }
}

void SkillsPageHandler::OnSkillsServiceShuttingDown() {
  first_party_download_timer_.Stop();
  On1PDownloadTimeout();
  service_observation_.Reset();
}

void SkillsPageHandler::Request1PSkills() {
  // If there is a download already running then don't process a new request.
  if (Is1PDownloadTimerRunning()) {
    RecordSkillsDownloadRequestStatus(
        SkillsDownloadRequestStatus::kAlreadyRunning);
    return;
  }

  if (auto* service =
          SkillsServiceFactory::GetForProfile(base::to_address(profile_))) {
    service->FetchDiscoverySkills();
    first_party_download_timer_.Start(FROM_HERE, kMax1PDownloadTimeout, this,
                                      &SkillsPageHandler::On1PDownloadTimeout);
    RecordSkillsDownloadRequestStatus(SkillsDownloadRequestStatus::kSent);
  }
}

void SkillsPageHandler::GetInitial1PSkills(
    GetInitial1PSkillsCallback callback) {
  auto scoped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), FirstPartySkillsMap());
  auto* service =
      SkillsServiceFactory::GetForProfile(base::to_address(profile_));
  std::move(scoped_callback).Run(Translate1PSkillsMap(service->Get1PSkills()));
}

void SkillsPageHandler::OnDiscoverySkillsUpdated(
    const SkillsService::SkillsMap* skills_map) {
  first_party_download_timer_.Stop();
  RecordSkillsDownloadRequestStatus(
      SkillsDownloadRequestStatus::kResponseReceived);
  if (pending_save_1p_request_.has_value()) {
    auto request = std::exchange(pending_save_1p_request_, std::nullopt);
    bool valid_skill = !skills_map || skills_map->contains(request->skill_id);
    if (!valid_skill) {
      RecordSkillsManagementError(SkillsManagementError::k1pSkillDNE);
    }
    std::move(request->callback).Run(valid_skill);
  }

  // If the map exists (even if empty) that means we have an updated list of
  // skills.
  if (skills_map) {
    page_->Update1PMap(Translate1PSkillsMap(*skills_map));
  }
}

void SkillsPageHandler::MaybeSave1PSkill(const std::string& skill_id,
                                         MaybeSave1PSkillCallback callback) {
  if (pending_save_1p_request_.has_value()) {
    receiver_.ReportBadMessage("Save call in progress");
    return;
  }
  pending_save_1p_request_.emplace(skill_id, std::move(callback));
  Request1PSkills();
}

void SkillsPageHandler::On1PDownloadTimeout() {
  if (pending_save_1p_request_.has_value()) {
    RecordSkillsDownloadRequestStatus(SkillsDownloadRequestStatus::kTimedOut);
    auto request = std::exchange(pending_save_1p_request_, std::nullopt);
    std::move(request->callback).Run(false);
  }
}

}  // namespace skills
