// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_dialog_handler.h"

#include <optional>

#include "base/check_deref.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_delegate.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_service.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/emoji/emoji_panel_helper.h"

namespace skills {
namespace {
using ::optimization_guide::ModelBasedCapabilityKey;
using ::optimization_guide::ModelExecutionOptions;
using ::optimization_guide::ModelQualityLogEntry;
using ::optimization_guide::OptimizationGuideModelExecutionResult;
using ::optimization_guide::proto::SkillsRequest;
using ::optimization_guide::proto::SkillsResponse;
using ::skills::mojom::DialogHandler;
}  // namespace

SkillsDialogHandler::SkillsDialogHandler(
    mojo::PendingReceiver<DialogHandler> receiver,
    content::WebContents* web_contents,
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    skills::Skill initial_skill,
    base::WeakPtr<SkillsDialogDelegate> delegate)
    : receiver_(this, std::move(receiver)),
      web_contents_(CHECK_DEREF(web_contents)),
      optimization_guide_keyed_service_(optimization_guide_keyed_service),
      initial_skill_(std::move(initial_skill)),
      delegate_(delegate),
      profile_(CHECK_DEREF(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {}

SkillsDialogHandler::~SkillsDialogHandler() = default;

const skills::Skill* SkillsDialogHandler::SaveOrUpdateSkill(
    const skills::Skill& skill) {
  auto* skills_service =
      SkillsServiceFactory::GetForProfile(base::to_address(profile_));
  if (!skills_service) {
    RecordSkillsSaveResult(SkillsSaveResult::kServiceNotFound);
    return nullptr;
  }
  if (skills_service->GetServiceStatus() !=
      SkillsService::ServiceStatus::kReady) {
    RecordSkillsSaveResult(SkillsSaveResult::kServiceNotReady);
    return nullptr;
  }
  const Skill* result = nullptr;
  if (skill.id.empty()) {
    result = skills_service->AddSkill(skill.source_skill_id, skill.name,
                                      skill.icon, skill.prompt);
    if (!result) {
      RecordSkillsSaveResult(SkillsSaveResult::kWriteFailed);
    }
  } else {
    result = skills_service->UpdateSkill(skill.id, skill.name, skill.icon,
                                         skill.prompt);
    if (!result) {
      RecordSkillsSaveResult(SkillsSaveResult::kSkillNotFound);
    }
  }
  return result;
}

void SkillsDialogHandler::SubmitSkill(
    const skills::Skill& skill,
    DialogHandler::SubmitSkillCallback callback) {
  auto wrapped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false);
  if (!delegate_) {
    RecordSkillsSaveResult(SkillsSaveResult::kUiContextLost);
    return;
  }
  const Skill* response = SaveOrUpdateSkill(skill);
  if (!response) {
    return;
  }
  // TODO(crbug.com/477385216): Update to use an enum for creation mode.
  RecordSkillsDialogAction(SkillsDialogAction::kSaved,
                           /*is_edit_mode=*/!initial_skill_.id.empty());
  // Triggers toast
  delegate_->OnSkillSaved(response->id);
  delegate_->CloseDialog();
  RecordSkillsSaveResult(SkillsSaveResult::kSuccess);
  std::move(wrapped_callback).Run(true);
}

void SkillsDialogHandler::CloseDialog() {
  // TODO(crbug.com/477385216): Update to use an enum for creation mode.
  RecordSkillsDialogAction(SkillsDialogAction::kCancelled,
                           /*is_edit_mode=*/!initial_skill_.id.empty());
  if (delegate_) {
    delegate_->CloseDialog();
  }
}

void SkillsDialogHandler::ShowEmojiPicker() {
  ui::ShowEmojiPanel();
}

void SkillsDialogHandler::GetInitialSkill(GetInitialSkillCallback callback) {
  std::move(callback).Run(initial_skill_);
}

void SkillsDialogHandler::OnRefineSkillResponse(
    DialogHandler::RefineSkillCallback callback,
    OptimizationGuideModelExecutionResult result,
    std::unique_ptr<ModelQualityLogEntry> log_entry) {
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::nullopt);

  if (!result.response.has_value()) {
    RecordSkillsRefineResult(SkillsRefineResult::kModelExecutionFailed);
    return;
  }

  // Parse the response into SkillsResponse proto
  auto response = optimization_guide::ParsedAnyMetadata<SkillsResponse>(
      result.response.value());

  if (!response) {
    RecordSkillsRefineResult(SkillsRefineResult::kParseError);
    return;
  }
  if (response->suggestions_size() == 0) {
    RecordSkillsRefineResult(SkillsRefineResult::kNoSuggestions);
    return;
  }

  // Get the first suggestion (which contains the refined prompt)
  const auto& suggestion = response->suggestions(0);

  // Map the proto data to Mojo Skill object
  skills::Skill refined_skill;
  refined_skill.prompt = suggestion.prompt();  // The refined prompt
  refined_skill.name = suggestion.name();      // Suggested name
  refined_skill.icon = suggestion.icon();      // Suggested icon/emoji

  RecordSkillsRefineResult(SkillsRefineResult::kSuccess);
  std::move(wrapped_callback).Run(std::move(refined_skill));
}

void SkillsDialogHandler::RefineSkill(
    const skills::Skill& skill,
    DialogHandler::RefineSkillCallback callback) {
  // TODO(crbug.com/477385216): Update to use an enum for creation mode.
  RecordSkillsDialogAction(SkillsDialogAction::kRefined,
                           /*is_edit_mode=*/!initial_skill_.id.empty());
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::nullopt);

  if (skill.prompt.empty()) {
    RecordSkillsRefineResult(SkillsRefineResult::kInvalidRequest);
    return;
  }
  if (!optimization_guide_keyed_service_) {
    RecordSkillsRefineResult(SkillsRefineResult::kServiceUnavailable);
    return;
  }

  SkillsRequest skills_request_proto;
  skills_request_proto.set_task_type(SkillsRequest::REFINE_PROMPT);

  auto* draft = skills_request_proto.mutable_skill_draft();
  draft->set_prompt(skill.prompt);
  draft->set_name(skill.name);

  optimization_guide_keyed_service_->ExecuteModel(
      ModelBasedCapabilityKey::kSkills, skills_request_proto,
      ModelExecutionOptions(),
      base::BindOnce(&SkillsDialogHandler::OnRefineSkillResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(wrapped_callback)));
}

void SkillsDialogHandler::GetSignedInEmail(GetSignedInEmailCallback callback) {
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::string());

  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(base::to_address(profile_));

  if (!identity_manager) {
    return;
  }

  CoreAccountInfo primary_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  std::move(wrapped_callback).Run(primary_account_info.email);
}

}  // namespace skills
