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
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace autofill_assistant {
// Static
std::unique_ptr<ScriptPrecondition> ScriptPrecondition::FromProto(
    const std::string& script_path,
    const ScriptPreconditionProto& script_precondition_proto) {
  std::vector<std::vector<std::string>> elements_exist;
  for (const auto& element : script_precondition_proto.elements_exist()) {
    std::vector<std::string> selectors;
    for (const auto& selector : element.selectors()) {
      selectors.emplace_back(selector);
    }
    if (selectors.empty()) {
      DLOG(WARNING)
          << "Empty selectors in script precondition for script path: "
          << script_path << ".";
      continue;
    }
    elements_exist.emplace_back(selectors);
  }

  std::set<std::string> domain_match;
  for (const auto& domain : script_precondition_proto.domain()) {
    domain_match.emplace(domain);
  }

  std::vector<std::unique_ptr<re2::RE2>> path_pattern;
  for (const auto& pattern : script_precondition_proto.path_pattern()) {
    auto re = std::make_unique<re2::RE2>(pattern);
    if (re->error_code() != re2::RE2::NoError) {
      DLOG(ERROR) << "Invalid regexp in script precondition '" << pattern
                  << "' for script path: " << script_path << ".";
      return nullptr;
    }
    path_pattern.emplace_back(std::move(re));
  }

  std::vector<ScriptParameterMatchProto> parameter_match;
  for (const auto& match : script_precondition_proto.script_parameter_match()) {
    parameter_match.emplace_back(match);
  }

  std::vector<FormValueMatchProto> form_value_match;
  for (const auto& match : script_precondition_proto.form_value_match()) {
    form_value_match.emplace_back(match);
  }

  std::vector<ScriptStatusMatchProto> status_matches;
  for (const auto& status_match :
       script_precondition_proto.script_status_match()) {
    status_matches.push_back(status_match);
  }

  // TODO(crbug.com/806868): Detect unknown or unsupported conditions and
  // reject them.
  return std::make_unique<ScriptPrecondition>(
      elements_exist, domain_match, std::move(path_pattern), parameter_match,
      form_value_match, status_matches);
}

ScriptPrecondition::~ScriptPrecondition() {}

void ScriptPrecondition::Check(
    const GURL& url,
    BatchElementChecker* batch_checks,
    const std::map<std::string, std::string>& parameters,
    const std::map<std::string, ScriptStatusProto>& executed_scripts,
    base::OnceCallback<void(bool)> callback) {
  if (!MatchDomain(url) || !MatchPath(url) || !MatchParameters(parameters) ||
      !MatchScriptStatus(executed_scripts)) {
    std::move(callback).Run(false);
    return;
  }

  pending_check_count_ = elements_exist_.size() + form_value_match_.size();
  if (pending_check_count_ == 0) {
    std::move(callback).Run(true);
    return;
  }

  check_preconditions_callback_ = std::move(callback);
  for (const auto& selector : elements_exist_) {
    base::OnceCallback<void(bool)> callback =
        base::BindOnce(&ScriptPrecondition::OnCheckElementExists,
                       weak_ptr_factory_.GetWeakPtr());
    batch_checks->AddElementCheck(kExistenceCheck, selector,
                                  std::move(callback));
  }
  for (const auto& value_match : form_value_match_) {
    DCHECK(!value_match.element().selectors().empty());
    std::vector<std::string> selectors;
    for (const auto& selector : value_match.element().selectors()) {
      selectors.emplace_back(selector);
    }
    DCHECK(!selectors.empty());

    batch_checks->AddFieldValueCheck(
        selectors, base::BindOnce(&ScriptPrecondition::OnGetFieldValue,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

ScriptPrecondition::ScriptPrecondition(
    const std::vector<std::vector<std::string>>& elements_exist,
    const std::set<std::string>& domain_match,
    std::vector<std::unique_ptr<re2::RE2>> path_pattern,
    const std::vector<ScriptParameterMatchProto>& parameter_match,
    const std::vector<FormValueMatchProto>& form_value_match,
    const std::vector<ScriptStatusMatchProto>& status_match)
    : elements_exist_(elements_exist),
      domain_match_(domain_match),
      path_pattern_(std::move(path_pattern)),
      parameter_match_(parameter_match),
      form_value_match_(form_value_match),
      status_match_(status_match),
      pending_check_count_(0),
      weak_ptr_factory_(this) {}

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
    if (regexp->Match(path, 0, path.size(), re2::RE2::UNANCHORED, NULL, 0)) {
      return true;
    }
  }
  return false;
}

bool ScriptPrecondition::MatchParameters(
    const std::map<std::string, std::string>& parameters) const {
  for (const auto& match : parameter_match_) {
    auto iter = parameters.find(match.name());
    if (match.exists()) {
      // parameter must exist and optionally have a specific value
      if (iter == parameters.end())
        return false;

      if (!match.value_equals().empty() && iter->second != match.value_equals())
        return false;

    } else {
      // parameter must not exist
      if (iter != parameters.end())
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

void ScriptPrecondition::OnCheckElementExists(bool exists) {
  ReportCheckResult(exists);
}

void ScriptPrecondition::OnGetFieldValue(bool exists,
                                         const std::string& value) {
  ReportCheckResult(!value.empty());
}

void ScriptPrecondition::ReportCheckResult(bool success) {
  if (!check_preconditions_callback_)
    return;

  if (!success) {
    std::move(check_preconditions_callback_).Run(false);
    return;
  }

  --pending_check_count_;
  if (pending_check_count_ <= 0) {
    DCHECK_EQ(pending_check_count_, 0);
    std::move(check_preconditions_callback_).Run(true);
  }
}

}  // namespace autofill_assistant
