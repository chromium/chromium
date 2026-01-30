// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_dialog_handler.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_delegate.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_service.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/emoji/emoji_panel_helper.h"

namespace skills {

SkillsDialogHandler::SkillsDialogHandler(
    mojo::PendingReceiver<skills::mojom::DialogHandler> receiver,
    content::WebContents* web_contents,
    base::WeakPtr<SkillsDialogDelegate> delegate)
    : receiver_(this, std::move(receiver)),
      web_contents_(web_contents),
      delegate_(delegate) {}

SkillsDialogHandler::~SkillsDialogHandler() = default;

void SkillsDialogHandler::SubmitSkill(const skills::Skill& skill) {
  if (auto* skills_service = SkillsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))) {
    skills_service->AddSkill(skill.name, skill.icon, skill.prompt);
    // TODO(marissashen): Add support for UpdateSkill
    if (delegate_) {
      delegate_->OnSkillSaved(skill.id);
      delegate_->CloseDialog();
    }
  } else {
    // TODO(marissashen): Add error handling.
    LOG(WARNING) << "SkillsPageHandler: SkillsService is null.";
  }
}

void SkillsDialogHandler::CloseDialog() {
  if (delegate_) {
    delegate_->CloseDialog();
  }
}

void SkillsDialogHandler::ShowEmojiPicker() {
  ui::ShowEmojiPanel();
}

}  // namespace skills
