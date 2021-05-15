// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PARAMETERS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PARAMETERS_H_

#include <map>
#include <string>

#include "components/autofill_assistant/browser/service.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Stores script parameters and provides access to the subset of client-relevant
// parameters.
class ScriptParameters {
 public:
  // TODO(arbesser): Expect properly typed parameters instead.
  ScriptParameters(const std::map<std::string, std::string>& parameters);
  ScriptParameters();
  ~ScriptParameters();
  ScriptParameters(const ScriptParameters&) = delete;
  ScriptParameters& operator=(const ScriptParameters&) = delete;

  // Merges |another| into this. Does not overwrite existing values.
  void MergeWith(const ScriptParameters& another);

  // Returns whether there is a script parameter that satisfies |proto|.
  bool Matches(const ScriptParameterMatchProto& proto) const;

  // Returns a proto representation of this class. If
  // |only_trigger_script_allowlisted| is set to true, this will only return the
  // list of trigger-script-approved script parameters.
  google::protobuf::RepeatedPtrField<ScriptParameterProto> ToProto(
      bool only_trigger_script_allowlisted = false) const;

  // Getters for specific parameters.
  absl::optional<std::string> GetOverlayColors() const;
  absl::optional<std::string> GetPasswordChangeUsername() const;
  absl::optional<std::string> GetBase64TriggerScriptsResponseProto() const;
  absl::optional<bool> GetRequestsTriggerScript() const;
  absl::optional<bool> GetStartImmediately() const;
  absl::optional<bool> GetEnabled() const;
  absl::optional<std::string> GetOriginalDeeplink() const;
  absl::optional<bool> GetTriggerScriptExperiment() const;
  absl::optional<std::string> GetIntent() const;
  absl::optional<std::string> GetCallerEmail() const;

  // Details parameters.
  absl::optional<bool> GetDetailsShowInitial() const;
  absl::optional<std::string> GetDetailsTitle() const;
  absl::optional<std::string> GetDetailsDescriptionLine1() const;
  absl::optional<std::string> GetDetailsDescriptionLine2() const;
  absl::optional<std::string> GetDetailsDescriptionLine3() const;
  absl::optional<std::string> GetDetailsImageUrl() const;
  absl::optional<std::string> GetDetailsImageAccessibilityHint() const;
  absl::optional<std::string> GetDetailsImageClickthroughUrl() const;
  absl::optional<std::string> GetDetailsTotalPriceLabel() const;
  absl::optional<std::string> GetDetailsTotalPrice() const;

 private:
  absl::optional<std::string> GetParameter(const std::string& name) const;

  std::map<std::string, std::string> parameters_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PARAMETERS_H_
