// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_precondition.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace {

void RunCallbackWithoutData(
    base::OnceCallback<void(bool)> callback,
    const ClientStatus& status,
    const std::vector<std::string>& ignored_payloads,
    const std::vector<std::string>& ignored_tags,
    const base::flat_map<std::string, DomObjectFrameStack>& ignored_elements) {
  std::move(callback).Run(status.ok());
}

}  // namespace

// Static
std::unique_ptr<ScriptPrecondition> ScriptPrecondition::FromProto(
    const std::string& script_path,
    const ScriptPreconditionProto& script_precondition_proto) {
  std::vector<std::unique_ptr<re2::RE2>> path_pattern;
  for (const auto& pattern : script_precondition_proto.path_pattern()) {
    auto re = std::make_unique<re2::RE2>(pattern);
    if (re->error_code() != re2::RE2::NoError) {
#ifdef NDEBUG
      DVLOG(1) << "Invalid regexp in script precondition";
#else
      DVLOG(1) << "Invalid regexp in script precondition '" << pattern
               << "' for script path: " << script_path << ".";
#endif

      return nullptr;
    }
    path_pattern.emplace_back(std::move(re));
  }

  // TODO(crbug.com/806868): Detect unknown or unsupported conditions and
  // reject them.
  return std::make_unique<ScriptPrecondition>(
      script_precondition_proto.domain(), std::move(path_pattern),
      script_precondition_proto.script_parameter_match(),
      script_precondition_proto.element_condition());
}

ScriptPrecondition::~ScriptPrecondition() {}

void ScriptPrecondition::Check(
    const GURL& url,
    BatchElementChecker* batch_checks,
    const TriggerContext& context,
    base::OnceCallback<void(bool)> callback) {
  if (!MatchDomain(url) || !MatchPath(url) || !MatchParameters(context)) {
    std::move(callback).Run(false);
    return;
  }
  batch_checks->AddElementConditionCheck(
      element_precondition_,
      base::BindOnce(&RunCallbackWithoutData, std::move(callback)));
}

ScriptPrecondition::ScriptPrecondition(
    const google::protobuf::RepeatedPtrField<std::string>& domain_match,
    std::vector<std::unique_ptr<re2::RE2>> path_pattern,
    const google::protobuf::RepeatedPtrField<ScriptParameterMatchProto>&
        parameter_match,
    const ElementConditionProto& element_condition)
    : domain_match_(domain_match.begin(), domain_match.end()),
      path_pattern_(std::move(path_pattern)),
      parameter_match_(parameter_match.begin(), parameter_match.end()),
      element_precondition_(element_condition) {}

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
    if (!context.GetScriptParameters().Matches(match)) {
      return false;
    }
  }
  return true;
}

}  // namespace autofill_assistant
