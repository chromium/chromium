// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_dialog_handler.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_service.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace skills {

SkillsDialogHandler::SkillsDialogHandler(
    mojo::PendingReceiver<skills::mojom::DialogHandler> receiver,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)), web_contents_(web_contents) {}

SkillsDialogHandler::~SkillsDialogHandler() = default;

void SkillsDialogHandler::SubmitSkill(skills::mojom::SkillPtr skill) {
  if (auto* skills_service = SkillsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))) {
    skills_service->AddSkill(skill->name, skill->icon, skill->prompt);
    // TODO: Call UI controller to close the dialog.
  } else {
    // TODO(marissashen): Add error handling.
    LOG(WARNING) << "SkillsDialogHandler: SkillsService is null.";
  }
}

void SkillsDialogHandler::CloseDialog() {
  // TODO: Call UI controller to close the dialog.
}

}  // namespace skills
