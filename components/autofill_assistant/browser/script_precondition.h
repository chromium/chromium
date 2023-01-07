// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PRECONDITION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PRECONDITION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
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
      const google::protobuf::RepeatedPtrField<ScriptParameterMatchProto>&
          parameter_match,
      const ElementConditionProto& must_match);

  ScriptPrecondition(const ScriptPrecondition&) = delete;
  ScriptPrecondition& operator=(const ScriptPrecondition&) = delete;

  ~ScriptPrecondition();

  // Check whether the conditions satisfied and return the result through
  // |callback|. |batch_checks| must remain valid until the callback is run.
  //
  // Calling Check() while another check is in progress cancels the previously
  // running check.
  void Check(const GURL& url,
             BatchElementChecker* batch_checks,
             const TriggerContext& context,
             base::OnceCallback<void(bool)> callback);

 private:
  bool MatchDomain(const GURL& url) const;
  bool MatchPath(const GURL& url) const;
  bool MatchParameters(const TriggerContext& context) const;

  // Domain (exact match) excluding the last '/' character.
  base::flat_set<std::string> domain_match_;

  // Pattern of the path parts of the URL.
  std::vector<std::unique_ptr<re2::RE2>> path_pattern_;

  // Condition on parameters, identified by name, as found in the intent.
  std::vector<ScriptParameterMatchProto> parameter_match_;

  ElementConditionProto element_precondition_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PRECONDITION_H_
