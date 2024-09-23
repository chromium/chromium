// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_FRAME_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_FRAME_HELPER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"

class GURL;

namespace password_manager {

class PasswordManagerClient;
class PasswordManagerDriver;

// Per-frame helper for password generation. Will enable this feature only if
//
// -  Password manager is enabled
// -  Password sync is enabled
//
// This class is used to determine what forms we should offer to generate
// passwords for and manages the popup which is created if the user chooses to
// generate a password.
class PasswordGenerationFrameHelper {
 public:
  PasswordGenerationFrameHelper(PasswordManagerClient* client,
                                PasswordManagerDriver* driver);

  PasswordGenerationFrameHelper(const PasswordGenerationFrameHelper&) = delete;
  PasswordGenerationFrameHelper& operator=(
      const PasswordGenerationFrameHelper&) = delete;

  virtual ~PasswordGenerationFrameHelper();

  // Instructs the PasswordRequirementsService to fetch requirements for
  // `origin`. This needs to be called to enable domain-wide password
  // requirements overrides.
  void PrefetchSpec(const GURL& origin);

  // Stores password requirements received from the autofill server for the
  // `form` and fetches domain-wide requirements.
  void ProcessPasswordRequirements(
      const autofill::FormData& form,
      const base::flat_map<autofill::FieldGlobalId,
                           autofill::AutofillType::ServerPrediction>&
          predictions);

  // Determines current state of password generation
  // `log_debug_data` determines whether log entries are sent to the
  // autofill::SavePasswordProgressLogger.
  //
  // Virtual for testing
  virtual bool IsGenerationEnabled(bool log_debug_data) const;

  // Returns true if `field_renderer_id` is in `generation_enabled_fields_` set.
  virtual bool IsManualGenerationEnabledField(
      autofill::FieldRendererId field_renderer_id) const;

  // Adds `field_renderer_id` to `generation_enabled_fields_` set.
  virtual void AddManualGenerationEnabledField(
      autofill::FieldRendererId field_renderer_id);

  // Returns a randomly generated password that should (but is not guaranteed
  // to) match the requirements of the site.
  // `last_committed_url` refers to the main frame URL and may impact the
  // password generation rules that are imposed by the site.
  // `form_signature` and `field_signature` identify the field for which a
  // password shall be generated.
  // `max_length` refers to the maximum allowed length according to the site and
  // may be 0 if unset.
  //
  // Virtual for testing
  //
  // TODO(crbug.com/41396292): Add a stub for this class to facilitate testing.
  virtual std::u16string GeneratePassword(
      const GURL& last_committed_url,
      autofill::password_generation::PasswordGenerationType generation_type,
      autofill::FormSignature form_signature,
      autofill::FieldSignature field_signature,
      uint64_t max_length);

 private:
  friend class PasswordGenerationFrameHelperTest;

  // The PasswordManagerClient instance associated with this instance. Must
  // outlive this instance.
  const raw_ptr<PasswordManagerClient> client_;

  // The PasswordManagerDriver instance associated with this instance. Must
  // outlive this instance.
  const raw_ptr<PasswordManagerDriver> driver_;

  // The fields that have manual generation enabled. This includes fields that
  // have type="text".
  base::flat_set<autofill::FieldRendererId> generation_enabled_fields_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_FRAME_HELPER_H_
