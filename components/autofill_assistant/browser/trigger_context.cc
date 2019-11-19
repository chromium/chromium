// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

// static
std::unique_ptr<TriggerContext> TriggerContext::CreateEmpty() {
  return std::make_unique<TriggerContextImpl>();
}

// static
std::unique_ptr<TriggerContext> TriggerContext::Create(
    std::map<std::string, std::string> params,
    const std::string& exp) {
  return std::make_unique<TriggerContextImpl>(params, exp);
}

// static
std::unique_ptr<TriggerContext> TriggerContext::Merge(
    std::vector<const TriggerContext*> contexts) {
  return std::make_unique<MergedTriggerContext>(contexts);
}

TriggerContext::TriggerContext() {}
TriggerContext::~TriggerContext() {}

TriggerContextImpl::TriggerContextImpl() {}

TriggerContextImpl::TriggerContextImpl(
    std::map<std::string, std::string> parameters,
    const std::string& experiment_ids)
    : parameters_(std::move(parameters)),
      experiment_ids_(std::move(experiment_ids)) {}
TriggerContextImpl::~TriggerContextImpl() = default;

void TriggerContextImpl::AddParameters(
    google::protobuf::RepeatedPtrField<ScriptParameterProto>* dest) const {
  for (const auto& param_entry : parameters_) {
    ScriptParameterProto* parameter = dest->Add();
    parameter->set_name(param_entry.first);
    parameter->set_value(param_entry.second);
  }
}

base::Optional<std::string> TriggerContextImpl::GetParameter(
    const std::string& name) const {
  auto iter = parameters_.find(name);
  if (iter == parameters_.end())
    return base::nullopt;

  return iter->second;
}

std::string TriggerContextImpl::experiment_ids() const {
  return experiment_ids_;
}

bool TriggerContextImpl::is_cct() const {
  return cct_;
}

bool TriggerContextImpl::is_onboarding_shown() const {
  return onboarding_shown_;
}

bool TriggerContextImpl::is_direct_action() const {
  return direct_action_;
}

MergedTriggerContext::MergedTriggerContext(
    std::vector<const TriggerContext*> contexts)
    : contexts_(contexts) {}

MergedTriggerContext::~MergedTriggerContext() {}

void MergedTriggerContext::AddParameters(
    google::protobuf::RepeatedPtrField<ScriptParameterProto>* dest) const {
  for (const TriggerContext* context : contexts_) {
    context->AddParameters(dest);
  }
}

base::Optional<std::string> MergedTriggerContext::GetParameter(
    const std::string& name) const {
  for (const TriggerContext* context : contexts_) {
    auto opt_value = context->GetParameter(name);
    if (opt_value)
      return opt_value;
  }
  return base::nullopt;
}

std::string MergedTriggerContext::experiment_ids() const {
  std::string experiment_ids;
  for (const TriggerContext* context : contexts_) {
    std::string context_experiment_ids = context->experiment_ids();
    if (context_experiment_ids.empty())
      continue;

    if (!experiment_ids.empty())
      experiment_ids.append(1, ',');

    experiment_ids.append(context->experiment_ids());
  }
  return experiment_ids;
}

bool MergedTriggerContext::is_cct() const {
  for (const TriggerContext* context : contexts_) {
    if (context->is_cct())
      return true;
  }
  return false;
}

bool MergedTriggerContext::is_onboarding_shown() const {
  for (const TriggerContext* context : contexts_) {
    if (context->is_onboarding_shown())
      return true;
  }
  return false;
}

bool MergedTriggerContext::is_direct_action() const {
  for (const TriggerContext* context : contexts_) {
    if (context->is_direct_action())
      return true;
  }
  return false;
}

}  // namespace autofill_assistant
