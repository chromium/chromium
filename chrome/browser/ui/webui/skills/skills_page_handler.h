// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/skills/skills.mojom.h"
#include "components/skills/public/skill.mojom-forward.h"
#include "components/skills/public/skills_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace skills {

class SkillsPageHandler : public skills::mojom::PageHandler {
 public:
  SkillsPageHandler(mojo::PendingReceiver<skills::mojom::PageHandler> receiver,
                    skills::SkillsService* skills_service);

  SkillsPageHandler(const SkillsPageHandler&) = delete;
  SkillsPageHandler& operator=(const SkillsPageHandler&) = delete;

  ~SkillsPageHandler() override;

  // skills::mojom::PageHandler:
  void SubmitSkill(skills::mojom::SkillPtr skill) override;
  void CloseDialog() override;

 private:
  mojo::Receiver<skills::mojom::PageHandler> receiver_;
  raw_ptr<skills::SkillsService> skills_service_;
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_PAGE_HANDLER_H_
