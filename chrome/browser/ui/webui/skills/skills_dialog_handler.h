// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/skills/skills.mojom.h"
#include "components/skills/public/skill.mojom-forward.h"
#include "components/skills/public/skills_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebContents;
}  // namespace content

namespace skills {

class SkillsDialogDelegate;

class SkillsDialogHandler : public skills::mojom::DialogHandler {
 public:
  SkillsDialogHandler(
      mojo::PendingReceiver<skills::mojom::DialogHandler> receiver,
      content::WebContents* web_contents,
      base::WeakPtr<SkillsDialogDelegate> delegate);

  SkillsDialogHandler(const SkillsDialogHandler&) = delete;
  SkillsDialogHandler& operator=(const SkillsDialogHandler&) = delete;

  ~SkillsDialogHandler() override;

  // skills::mojom::DialogHandler:
  void SubmitSkill(const skills::Skill& skill) override;
  void CloseDialog() override;
  void ShowEmojiPicker() override;

 private:
  mojo::Receiver<skills::mojom::DialogHandler> receiver_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  base::WeakPtr<SkillsDialogDelegate> delegate_;
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_HANDLER_H_
