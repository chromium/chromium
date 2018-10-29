// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/signatures_util.h"
#include "url/gurl.h"

namespace autofill {
class FormStructure;
}

namespace password_manager {

class PasswordManagerClient;
class PasswordManagerDriver;

// Per-tab manager for password generation. Will enable this feature only if
//
// -  Password manager is enabled
// -  Password sync is enabled
//
// NOTE: At the moment, the creation of the renderer PasswordGenerationManager
// is controlled by a switch (--enable-password-generation) so this feature will
// not be enabled regardless of the above criteria without the switch being
// present.
//
// This class is used to determine what forms we should offer to generate
// passwords for and manages the popup which is created if the user chooses to
// generate a password.
class PasswordGenerationManager {
 public:
  PasswordGenerationManager(PasswordManagerClient* client,
                            PasswordManagerDriver* driver);
  virtual ~PasswordGenerationManager();

  // Instructs the PasswordRequirementsService to fetch requirements for
  // |origin|. This needs to be called to enable domain-wide password
  // requirements overrides.
  void PrefetchSpec(const GURL& origin);

  // Stores password requirements received from the autofill server for the
  // |forms| and fetches domain-wide requirements.
  void ProcessPasswordRequirements(
      const std::vector<autofill::FormStructure*>& forms);

  // Detect account creation forms from forms with autofill type annotated.
  // Will send a message to the renderer if we find a correctly annotated form
  // and the feature is enabled.
  void DetectFormsEligibleForGeneration(
      const std::vector<autofill::FormStructure*>& forms);

  // Determines current state of password generation
  // |log_debug_data| determines whether log entries are sent to the
  // autofill::SavePasswordProgressLogger.
  bool IsGenerationEnabled(bool log_debug_data) const;

  // Returns a randomly generated password that should (but is not guaranteed
  // to) match the requirements of the site.
  // |last_committed_url| refers to the main frame URL and may impact the
  // password generation rules that are imposed by the site.
  // |form_signature| and |field_signature| identify the field for which a
  // password shall be generated.
  // |max_length| refers to the maximum allowed length according to the site and
  // may be 0 if unset.
  //
  // Virtual for testing
  //
  // TODO(crbug.com/855595): Add a stub for this class to facilitate testing.
  virtual base::string16 GeneratePassword(
      const GURL& last_committed_url,
      autofill::FormSignature form_signature,
      autofill::FieldSignature field_signature,
      uint32_t max_length,
      uint32_t* spec_priority);

 private:
  friend class PasswordGenerationManagerTest;

  // The PasswordManagerClient instance associated with this instance. Must
  // outlive this instance.
  PasswordManagerClient* client_;

  // The PasswordManagerDriver instance associated with this instance. Must
  // outlive this instance.
  PasswordManagerDriver* driver_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_MANAGER_H_
