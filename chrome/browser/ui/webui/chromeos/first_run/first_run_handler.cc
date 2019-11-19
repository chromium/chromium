// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/first_run/first_run_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/browser/web_ui.h"

namespace {

bool IsAssistantAllowed() {
  return ash::mojom::AssistantAllowedState::ALLOWED ==
         assistant::IsAssistantAllowedForProfile(
             ProfileManager::GetActiveUserProfile());
}

}  // namespace

namespace chromeos {

FirstRunHandler::FirstRunHandler()
    : is_initialized_(false),
      is_finalizing_(false) {
}

bool FirstRunHandler::IsInitialized() {
  return is_initialized_;
}

void FirstRunHandler::SetBackgroundVisible(bool visible) {
  web_ui()->CallJavascriptFunctionUnsafe("cr.FirstRun.setBackgroundVisible",
                                         base::Value(visible));
}

void FirstRunHandler::AddRectangularHole(int x, int y, int width, int height) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "cr.FirstRun.addRectangularHole", base::Value(x), base::Value(y),
      base::Value(width), base::Value(height));
}

void FirstRunHandler::AddRoundHole(int x, int y, float radius) {
  web_ui()->CallJavascriptFunctionUnsafe("cr.FirstRun.addRoundHole",
                                         base::Value(x), base::Value(y),
                                         base::Value(radius));
}

void FirstRunHandler::RemoveBackgroundHoles() {
  web_ui()->CallJavascriptFunctionUnsafe("cr.FirstRun.removeHoles");
}

void FirstRunHandler::ShowStepPositioned(const std::string& name,
                                         const StepPosition& position) {
  base::DictionaryValue step_params;
  step_params.SetKey("name", base::Value(name));
  step_params.SetKey("position", position.AsValue());
  step_params.SetKey("pointWithOffset", base::Value(base::Value::Type::LIST));
  step_params.SetKey("assistantEnabled", base::Value(IsAssistantAllowed()));

  web_ui()->CallJavascriptFunctionUnsafe("cr.FirstRun.showStep", step_params);
}

void FirstRunHandler::ShowStepPointingTo(const std::string& name,
                                         int x,
                                         int y,
                                         int offset) {
  base::DictionaryValue step_params;
  step_params.SetKey("name", base::Value(name));
  step_params.SetKey("position", base::Value());
  base::ListValue point_with_offset;
  point_with_offset.AppendInteger(x);
  point_with_offset.AppendInteger(y);
  point_with_offset.AppendInteger(offset);
  step_params.SetKey("pointWithOffset", std::move(point_with_offset));
  step_params.SetKey("assistantEnabled", base::Value(IsAssistantAllowed()));

  web_ui()->CallJavascriptFunctionUnsafe("cr.FirstRun.showStep", step_params);
}

void FirstRunHandler::HideCurrentStep() {
  web_ui()->CallJavascriptFunctionUnsafe("cr.FirstRun.hideCurrentStep");
}

void FirstRunHandler::Finalize() {
  is_finalizing_ = true;
  web_ui()->CallJavascriptFunctionUnsafe("cr.FirstRun.finalize");
}

bool FirstRunHandler::IsFinalizing() {
  return is_finalizing_;
}

void FirstRunHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialized", base::BindRepeating(&FirstRunHandler::HandleInitialized,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "nextButtonClicked",
      base::BindRepeating(&FirstRunHandler::HandleNextButtonClicked,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "helpButtonClicked",
      base::BindRepeating(&FirstRunHandler::HandleHelpButtonClicked,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stepShown", base::BindRepeating(&FirstRunHandler::HandleStepShown,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "stepHidden", base::BindRepeating(&FirstRunHandler::HandleStepHidden,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "finalized", base::BindRepeating(&FirstRunHandler::HandleFinalized,
                                       base::Unretained(this)));
}

void FirstRunHandler::HandleInitialized(const base::ListValue* args) {
  is_initialized_ = true;
  if (delegate())
    delegate()->OnActorInitialized();
}

void FirstRunHandler::HandleNextButtonClicked(const base::ListValue* args) {
  std::string step_name;
  CHECK(args->GetString(0, &step_name));
  if (delegate())
    delegate()->OnNextButtonClicked(step_name);
}

void FirstRunHandler::HandleHelpButtonClicked(const base::ListValue* args) {
  if (delegate())
    delegate()->OnHelpButtonClicked();
}

void FirstRunHandler::HandleStepShown(const base::ListValue* args) {
  std::string step_name;
  CHECK(args->GetString(0, &step_name));
  if (delegate())
    delegate()->OnStepShown(step_name);
}

void FirstRunHandler::HandleStepHidden(const base::ListValue* args) {
  std::string step_name;
  CHECK(args->GetString(0, &step_name));
  if (delegate())
    delegate()->OnStepHidden(step_name);
}

void FirstRunHandler::HandleFinalized(const base::ListValue* args) {
  is_finalizing_ = false;
  if (delegate())
    delegate()->OnActorFinalized();
}

}  // namespace chromeos
