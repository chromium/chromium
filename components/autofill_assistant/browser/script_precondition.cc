// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_precondition.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Static
std::unique_ptr<ScriptPrecondition> ScriptPrecondition::FromProto(
    const std::string& script_path,
    const ScriptPreconditionProto& script_precondition_proto) {
  std::vector<std::unique_ptr<re2::RE2>> path_pattern;
  for (const auto& pattern : script_precondition_proto.path_pattern()) {
    auto re = std::make_unique<re2::RE2>(pattern);
    if (re->error_code() != re2::RE2::NoError) {
      DVLOG(1) << "Invalid regexp in script precondition '" << pattern
               << "' for script path: " << script_path << ".";
      return nullptr;
    }
    path_pattern.emplace_back(std::move(re));
  }

  // TODO(crbug.com/806868): Detect unknown or unsupported conditions and
  // reject them.
  return std::make_unique<ScriptPrecondition>(
      script_precondition_proto.domain(), std::move(path_pattern),
      script_precondition_proto.script_status_match(),
      script_precondition_proto.script_parameter_match(),
      script_precondition_proto.elements_exist(),
      script_precondition_proto.form_value_match());
}

ScriptPrecondition::~ScriptPrecondition() {}

void ScriptPrecondition::Check(
    const GURL& url,
    BatchElementChecker* batch_checks,
    const TriggerContext& context,
    const std::map<std::string, ScriptStatusProto>& executed_scripts,
    base::OnceCallback<void(bool)> callback) {
  if (!MatchDomain(url) || !MatchPath(url) || !MatchParameters(context) ||
      !MatchScriptStatus(executed_scripts)) {
    std::move(callback).Run(false);
    return;
  }
  element_precondition_.Check(batch_checks, std::move(callback));
}

ScriptPrecondition::ScriptPrecondition(
    const google::protobuf::RepeatedPtrField<std::string>& domain_match,
    std::vector<std::unique_ptr<re2::RE2>> path_pattern,
    const google::protobuf::RepeatedPtrField<ScriptStatusMatchProto>&
        status_match,
    const google::protobuf::RepeatedPtrField<ScriptParameterMatchProto>&
        parameter_match,
    const google::protobuf::RepeatedPtrField<ElementReferenceProto>&
        element_exists,
    const google::protobuf::RepeatedPtrField<FormValueMatchProto>&
        form_value_match)
    : domain_match_(domain_match.begin(), domain_match.end()),
      path_pattern_(std::move(path_pattern)),
      parameter_match_(parameter_match.begin(), parameter_match.end()),
      status_match_(status_match.begin(), status_match.end()),
      element_precondition_(element_exists, form_value_match) {}

bool ScriptPrecondition::MatchDomain(const GURL& url) const {
  if (domain_match_.empty())
    return true;

  // We require the scheme and host parts to match.
  // TODO(crbug.com/806868): Consider using Origin::IsSameOriginWith here.
  std::string scheme_domain = base::StrCat({url.scheme(), "://", url.host()});
  return domain_match_.find(scheme_domain) != domain_match_.end();
}

bool ScriptPrecondition::MatchPath(const GURL& url) const {
  if (path_pattern_.empty()) {
    return true;
  }

  std::string path = url.has_ref()
                         ? base::StrCat({url.PathForRequest(), "#", url.ref()})
                         : url.PathForRequest();
  for (auto& regexp : path_pattern_) {
    if (regexp->Match(path, 0, path.size(), re2::RE2::ANCHOR_BOTH, NULL, 0)) {
      return true;
    }
  }
  return false;
}

bool ScriptPrecondition::MatchParameters(const TriggerContext& context) const {
  for (const auto& match : parameter_match_) {
    auto opt_value = context.GetParameter(match.name());
    if (match.exists()) {
      // parameter must exist and optionally have a specific value
      if (!opt_value)
        return false;

      if (!match.value_equals().empty() &&
          opt_value.value() != match.value_equals())
        return false;

    } else {
      // parameter must not exist
      if (opt_value)
        return false;
    }
  }
  return true;
}

bool ScriptPrecondition::MatchScriptStatus(
    const std::map<std::string, ScriptStatusProto>& executed_scripts) const {
  for (const auto status_match : status_match_) {
    auto status = SCRIPT_STATUS_NOT_RUN;
    auto iter = executed_scripts.find(status_match.script());
    if (iter != executed_scripts.end()) {
      status = iter->second;
    }
    bool has_same_status = status_match.status() == status;
    switch (status_match.comparator()) {
      case ScriptStatusMatchProto::DIFFERENT:
        if (has_same_status)
          return false;
        break;
      case ScriptStatusMatchProto::EQUAL:
      default:
        if (!has_same_status)
          return false;
        break;
    }
  }
  return true;
}

}  // namespace autofill_assistant
