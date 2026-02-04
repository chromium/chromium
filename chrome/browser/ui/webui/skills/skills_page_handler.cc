// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom.h"
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
    translated_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
    translated_map[skill.category()].push_back(std::move(translated_skill));
  }
  return translated_map;
}

}  // namespace

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
  }
}

void SkillsPageHandler::GetInitialUserSkills(
    GetInitialUserSkillsCallback callback) {
  std::vector<skills::Skill> skills;
  auto scoped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::vector<skills::Skill>());
  auto* service =
      SkillsServiceFactory::GetForProfile(base::to_address(profile_));
  if (!service || !service->IsInitialized()) {
    return;
  }

  for (const auto& skill : service->GetSkills()) {
    skills.push_back(*skill);
  }
  std::move(scoped_callback).Run(std::move(skills));
}

void SkillsPageHandler::OnSkillUpdated(
    std::string_view skill_id,
    SkillsService::UpdateSource update_source) {
  if (auto* service =
          SkillsServiceFactory::GetForProfile(base::to_address(profile_))) {
    const auto* skill = service->GetSkillById(skill_id);
    if (skill) {
      // If the skill exists, this means the skill was either added or updated.
      page_->UpdateSkill(*skill);
    } else {
      // If the skill no longer exists, this means the skill was deleted.
      page_->RemoveSkill(std::string(skill_id));
    }
  }
}

void SkillsPageHandler::OnSkillsServiceShuttingDown() {
  service_observation_.Reset();
}

void SkillsPageHandler::Request1PSkills() {
  if (auto* service =
          SkillsServiceFactory::GetForProfile(base::to_address(profile_))) {
    service->FetchDiscoverySkills();
  }
}

void SkillsPageHandler::GetInitial1PSkills(
    GetInitial1PSkillsCallback callback) {
  auto scoped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), FirstPartySkillsMap());
  auto* service =
      SkillsServiceFactory::GetForProfile(base::to_address(profile_));
  if (!service || !service->IsInitialized()) {
    return;
  }
  std::move(scoped_callback).Run(Translate1PSkillsMap(service->Get1PSkills()));
}

void SkillsPageHandler::OnDiscoverySkillsUpdated(
    const SkillsService::SkillsMap* skills_map) {
  // TODO(b/479029101): Handle the discover skill save error state.
  if (!skills_map) {
    return;
  }

  // If the map exists (even if empty) that means we have an updated list of
  // skills.
  page_->Update1PMap(Translate1PSkillsMap(*skills_map));
}

}  // namespace skills
