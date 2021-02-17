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

  // Returns a proto representation of this class. If
  // |only_trigger_script_allowlisted| is set to true, this will only return the
  // list of trigger-script-approved script parameters.
  google::protobuf::RepeatedPtrField<ScriptParameterProto> ToProto(
      bool only_trigger_script_allowlisted = false) const;

  // Returns the value of a specific parameter, if present.
  // TODO(arbesser): Remove this, provide getters for relevant params instead.
  base::Optional<std::string> GetParameter(const std::string& name) const;

  // Getters for specific parameters.
  base::Optional<std::string> GetOverlayColors() const;
  base::Optional<std::string> GetPasswordChangeUsername() const;
  base::Optional<std::string> GetBase64TriggerScriptsResponseProto() const;

 private:
  std::map<std::string, std::string> parameters_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_PARAMETERS_H_
