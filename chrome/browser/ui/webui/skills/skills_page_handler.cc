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

}  // namespace skills
