// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/ui/webui/skills/skills.mojom.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom-forward.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

namespace skills {

class SkillsDialogDelegate;

class SkillsDialogHandler : public skills::mojom::DialogHandler {
 public:
  SkillsDialogHandler(
      mojo::PendingReceiver<skills::mojom::DialogHandler> receiver,
      content::WebContents* web_contents,
      OptimizationGuideKeyedService* optimization_guide_keyed_service,
      skills::Skill initial_skill,
      base::WeakPtr<SkillsDialogDelegate> delegate);

  SkillsDialogHandler(const SkillsDialogHandler&) = delete;
  SkillsDialogHandler& operator=(const SkillsDialogHandler&) = delete;

  ~SkillsDialogHandler() override;

  // skills::mojom::DialogHandler:
  void SubmitSkill(
      const skills::Skill& skill,
      skills::mojom::DialogHandler::SubmitSkillCallback callback) override;
  void CloseDialog() override;
  void ShowEmojiPicker() override;
  void GetInitialSkill(GetInitialSkillCallback callback) override;
  void RefineSkill(
      const skills::Skill& skill,
      skills::mojom::DialogHandler::RefineSkillCallback callback) override;
  void GetSignedInEmail(GetSignedInEmailCallback callback) override;

 protected:
  virtual const skills::Skill* SaveOrUpdateSkill(const skills::Skill& skill);

 private:
  // Callback for the model execution result for `RefineSkill`.
  void OnRefineSkillResponse(
      skills::mojom::DialogHandler::RefineSkillCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  mojo::Receiver<skills::mojom::DialogHandler> receiver_;
  const raw_ref<content::WebContents> web_contents_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_ =
      nullptr;
  // The skill data used to pre-populate the dialog's input fields.
  skills::Skill initial_skill_;
  base::WeakPtr<SkillsDialogDelegate> delegate_;

  // Initialized with the browser_context passed in the constructor.
  const raw_ref<Profile> profile_;
  base::WeakPtrFactory<SkillsDialogHandler> weak_ptr_factory_{this};
};

}  // namespace skills

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_DIALOG_HANDLER_H_
