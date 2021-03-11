// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PARAMETERS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PARAMETERS_H_

#include <map>
#include <string>

#include "base/optional.h"
#include "components/autofill_assistant/browser/service.pb.h"

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
  base::Optional<std::string> GetOverlayColors() const;
  base::Optional<std::string> GetPasswordChangeUsername() const;
  base::Optional<std::string> GetBase64TriggerScriptsResponseProto() const;
  base::Optional<bool> GetRequestsTriggerScript() const;
  base::Optional<bool> GetStartImmediately() const;
  base::Optional<bool> GetEnabled() const;
  base::Optional<std::string> GetIntent() const;

  // Details parameters.
  base::Optional<bool> GetDetailsShowInitial() const;
  base::Optional<std::string> GetDetailsTitle() const;
  base::Optional<std::string> GetDetailsDescriptionLine1() const;
  base::Optional<std::string> GetDetailsDescriptionLine2() const;
  base::Optional<std::string> GetDetailsDescriptionLine3() const;
  base::Optional<std::string> GetDetailsImageUrl() const;
  base::Optional<std::string> GetDetailsImageAccessibilityHint() const;
  base::Optional<std::string> GetDetailsImageClickthroughUrl() const;
  base::Optional<std::string> GetDetailsTotalPriceLabel() const;
  base::Optional<std::string> GetDetailsTotalPrice() const;

 private:
  base::Optional<std::string> GetParameter(const std::string& name) const;

  std::map<std::string, std::string> parameters_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PARAMETERS_H_
