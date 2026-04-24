// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler.h"

#include "base/check_deref.h"
#include "base/types/optional_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_window_controller.h"
#include "chrome/browser/ui/webui/skills/skills.mojom-shared.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_types.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace skills {
namespace {

SkillCategoryToSkillMap Translate1PSkills(const SkillProtoList& skills_list) {
  SkillCategoryToSkillMap translated_map;
  for (const auto& skill : skills_list) {
    Skill translated_skill;
    translated_skill.id = skill.id();
    translated_skill.name = skill.name();
    translated_skill.icon = skill.icon();
    translated_skill.prompt = skill.prompt();
    translated_skill.description = skill.description();
    translated_skill.image_url = GURL(skill.image_url());
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

std::optional<std::vector<skills::Skill>> GetSortedUserSkills(
    Profile* profile) {
  auto* service =
      SkillsServiceFactory::GetForProfile(base::to_address(profile));
  if (!IsServiceReady(service)) {
    return std::nullopt;
  }

  std::vector<skills::Skill> skills;
  for (const auto& skill : service->GetSkills()) {
    skills.push_back(*skill);
  }
  return skills;
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
    tab_controller->ShowDialog(
        skill.value_or(skills::Skill()),
        ResolveEntryPointForManagementPage(base::OptionalToPtr(skill)),
        dialog_type);
  } else {
    RecordSkillsManagementError(SkillsManagementError::kTabControllerDNE);
  }
}

void SkillsPageHandler::GetInitialUserSkills(
    GetInitialUserSkillsCallback callback) {
  auto scoped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::vector<skills::Skill>());
  std::optional<std::vector<skills::Skill>> skills =
      GetSortedUserSkills(base::to_address(profile_));
  if (!skills) {
    return;
  }
  std::move(scoped_callback).Run(std::move(skills.value()));
}

void SkillsPageHandler::DeleteSkill(const std::string& skill_id) {
  // Show a toast to the user that the skill was deleted.
  auto* tabs = tabs::TabInterface::GetFromContents(&web_contents_.get());
  if (auto* browser_window_interface = tabs->GetBrowserWindowInterface()) {
    SkillsUiWindowController::From(browser_window_interface)
        ->OnSkillDeleted(skill_id);
  }
}

void SkillsPageHandler::OnTemporarySkillDisplay(
    std::string_view skill_id,
    SkillsService::DisplayState display_state) {
  switch (display_state) {
    case SkillsService::DisplayState::kDeleted:
      // Notify the UI that the skill was deleted but we don't notify the
      // service since a user can undo the deletion.
      page_->RemoveSkill(std::string(skill_id));
      break;
    case SkillsService::DisplayState::kReshown:
      if (auto* service =
              SkillsServiceFactory::GetForProfile(base::to_address(profile_))) {
        // The skill must exist at this point since we should not have deleted
        // it yet.
        const auto* skill = service->GetSkillById(skill_id);
        CHECK(skill);
        std::optional<std::vector<skills::Skill>> skills =
            GetSortedUserSkills(base::to_address(profile_));
        // This means the service is not ready, so we can't update the skills.
        if (!skills) {
          return;
        }
        page_->UpdateSkills(skills.value());
      }
      break;
    default:
      NOTREACHED();
  }
}

void SkillsPageHandler::OnSkillUpdated(
    std::string_view skill_id,
    SkillsService::UpdateSource update_source,
    bool is_position_changed) {
  if (auto* service =
          SkillsServiceFactory::GetForProfile(base::to_address(profile_))) {
    const auto* skill = service->GetSkillById(skill_id);
    if (skill) {
      // If the skill exists, this means a skill was either added or updated.
      // We need to update the entire list of skills since the order may have
      // changed.
      std::optional<std::vector<skills::Skill>> skills =
          GetSortedUserSkills(base::to_address(profile_));
      // This means the service is not ready, so we can't update the skills.
      if (!skills) {
        return;
      }
      page_->UpdateSkills(skills.value());
    } else {
      // If the skill no longer exists, this means the skill was deleted.
      page_->RemoveSkill(std::string(skill_id));
    }
  }
}

void SkillsPageHandler::OnSkillsServiceShuttingDown() {
  first_party_download_timer_.Stop();
  On1PDownloadTimeout();
  service_observation_.Reset();
}

bool SkillsPageHandler::Require1PSkillRefresh() {
  return true;
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
      std::move(callback), SkillCategoryToSkillMap());
  auto* service =
      SkillsServiceFactory::GetForProfile(base::to_address(profile_));
  std::move(scoped_callback).Run(Translate1PSkills(service->Get1PSkills()));
}

void SkillsPageHandler::OnDiscoverySkillsUpdated(
    const FirstPartySkillData* first_party_skill_data) {
  first_party_download_timer_.Stop();
  RecordSkillsDownloadRequestStatus(
      SkillsDownloadRequestStatus::kResponseReceived);
  if (pending_save_1p_request_.has_value()) {
    auto request = std::exchange(pending_save_1p_request_, std::nullopt);
    bool valid_skill =
        !first_party_skill_data ||
        std::find_if(first_party_skill_data->skills_list.begin(),
                     first_party_skill_data->skills_list.end(),
                     [&](const auto& skill) {
                       return skill.id() == request->skill_id;
                     }) != first_party_skill_data->skills_list.end();
    if (!valid_skill) {
      RecordSkillsManagementError(SkillsManagementError::k1pSkillDNE);
    }
    std::move(request->callback).Run(valid_skill);
  }

  // If the data exists that means we have an updated list of skills.
  if (first_party_skill_data) {
    page_->Update1PMap(Translate1PSkills(first_party_skill_data->skills_list));
    // TODO (crbug.com/503394871): Notify the UI about the topics list.
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

void SkillsPageHandler::RecordSkillsManagementAction(
    skills::mojom::SkillsManagementPage page,
    skills::mojom::SkillsManagementAction action) {
  skills::RecordSkillsManagementAction(page, action);
}

}  // namespace skills
