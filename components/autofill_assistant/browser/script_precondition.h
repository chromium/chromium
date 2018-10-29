// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PRECONDITION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PRECONDITION_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web_controller.h"

namespace re2 {
class RE2;
}  // namespace re2

namespace autofill_assistant {
class BatchElementChecker;

// Class represents a set of preconditions for a script to be executed.
class ScriptPrecondition {
 public:
  // Builds a precondition from its proto representation. Returns nullptr if the
  // preconditions are invalid.
  //
  // Note: The |script_path| paramter is used to allow logging misconfigured
  // scripts.
  static std::unique_ptr<ScriptPrecondition> FromProto(
      const std::string& script_path,
      const ScriptPreconditionProto& script_precondition_proto);

  ScriptPrecondition(
      const std::vector<std::vector<std::string>>& elements_exist,
      const std::set<std::string>& domain_match,
      std::vector<std::unique_ptr<re2::RE2>> path_pattern,
      const std::vector<ScriptParameterMatchProto>& parameter_match,
      const std::vector<FormValueMatchProto>& form_value_match,
      const std::vector<ScriptStatusMatchProto>& status_match);
  ~ScriptPrecondition();

  // Check whether the conditions satisfied and return the result through
  // |callback|. |batch_checks| must remain valid until the callback is run.
  //
  // Calling Check() while another check is in progress cancels the previously
  // running check.
  void Check(const GURL& url,
             BatchElementChecker* batch_checks,
             const std::map<std::string, std::string>& parameters,
             const std::map<std::string, ScriptStatusProto>& executed_scripts,
             base::OnceCallback<void(bool)> callback);

 private:
  bool MatchDomain(const GURL& url) const;
  bool MatchPath(const GURL& url) const;
  bool MatchParameters(
      const std::map<std::string, std::string>& parameters) const;
  bool MatchScriptStatus(
      const std::map<std::string, ScriptStatusProto>& executed_scripts) const;

  void OnCheckElementExists(bool exists);
  void OnGetFieldValue(bool exists, const std::string& value);
  void ReportCheckResult(bool success);

  std::vector<std::vector<std::string>> elements_exist_;

  // Domain (exact match) excluding the last '/' character.
  std::set<std::string> domain_match_;

  // Pattern of the path parts of the URL.
  std::vector<std::unique_ptr<re2::RE2>> path_pattern_;

  // Condition on parameters, identified by name, as found in the intent.
  std::vector<ScriptParameterMatchProto> parameter_match_;

  // Conditions on form fields value.
  std::vector<FormValueMatchProto> form_value_match_;

  // Conditions regarding the execution status of passed scripts.
  std::vector<ScriptStatusMatchProto> status_match_;

  // Number of checks for which there's still no result.
  int pending_check_count_;

  // A callback called as soon as an element or field check fails or, failing
  // that, when |pending_check_count_| reaches 0.
  base::OnceCallback<void(bool)> check_preconditions_callback_;

  base::WeakPtrFactory<ScriptPrecondition> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScriptPrecondition);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PRECONDITION_H_
