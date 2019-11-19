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
#include "components/autofill_assistant/browser/element_precondition.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace re2 {
class RE2;
}  // namespace re2

namespace autofill_assistant {
class BatchElementChecker;
class TriggerContext;

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
      const google::protobuf::RepeatedPtrField<std::string>& domain_match,
      std::vector<std::unique_ptr<re2::RE2>> path_pattern,
      const google::protobuf::RepeatedPtrField<ScriptStatusMatchProto>&
          status_match,
      const google::protobuf::RepeatedPtrField<ScriptParameterMatchProto>&
          parameter_match,
      const google::protobuf::RepeatedPtrField<ElementReferenceProto>&
          element_exists,
      const google::protobuf::RepeatedPtrField<FormValueMatchProto>&
          form_value_match);

  ~ScriptPrecondition();

  // Check whether the conditions satisfied and return the result through
  // |callback|. |batch_checks| must remain valid until the callback is run.
  //
  // Calling Check() while another check is in progress cancels the previously
  // running check.
  void Check(const GURL& url,
             BatchElementChecker* batch_checks,
             const TriggerContext& context,
             const std::map<std::string, ScriptStatusProto>& executed_scripts,
             base::OnceCallback<void(bool)> callback);

 private:
  bool MatchDomain(const GURL& url) const;
  bool MatchPath(const GURL& url) const;
  bool MatchParameters(const TriggerContext& context) const;
  bool MatchScriptStatus(
      const std::map<std::string, ScriptStatusProto>& executed_scripts) const;

  // Domain (exact match) excluding the last '/' character.
  std::set<std::string> domain_match_;

  // Pattern of the path parts of the URL.
  std::vector<std::unique_ptr<re2::RE2>> path_pattern_;

  // Condition on parameters, identified by name, as found in the intent.
  std::vector<ScriptParameterMatchProto> parameter_match_;

  // Conditions regarding the execution status of passed scripts.
  std::vector<ScriptStatusMatchProto> status_match_;

  ElementPrecondition element_precondition_;

  DISALLOW_COPY_AND_ASSIGN(ScriptPrecondition);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PRECONDITION_H_
