// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler.h"

#include "base/logging.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace skills {

SkillsPageHandler::SkillsPageHandler(
    mojo::PendingReceiver<skills::mojom::PageHandler> receiver,
    skills::SkillsService* skills_service)
    : receiver_(this, std::move(receiver)), skills_service_(skills_service) {}

SkillsPageHandler::~SkillsPageHandler() = default;

void SkillsPageHandler::SubmitSkill(skills::mojom::SkillPtr skill) {
  if (skills_service_) {
    // TODO(marissashen): Add support for UpdateSkill
    skills_service_->AddSkill(skill->name, skill->icon, skill->prompt);
    // TODO: Call UI controller to close the dialog.
  } else {
    // TODO(marissashen): Add error handling.
    LOG(WARNING) << "SkillsPageHandler: SkillsService is null.";
  }
}

void SkillsPageHandler::CloseDialog() {
  // TODO: Call UI controller to close the dialog.
}

}  // namespace skills
