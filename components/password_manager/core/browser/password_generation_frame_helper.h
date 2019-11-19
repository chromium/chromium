// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_FRAME_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_FRAME_HELPER_H_

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
  virtual ~PasswordGenerationFrameHelper();

  // Instructs the PasswordRequirementsService to fetch requirements for
  // |origin|. This needs to be called to enable domain-wide password
  // requirements overrides.
  void PrefetchSpec(const GURL& origin);

  // Stores password requirements received from the autofill server for the
  // |forms| and fetches domain-wide requirements.
  void ProcessPasswordRequirements(
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
  friend class PasswordGenerationFrameHelperTest;

  // The PasswordManagerClient instance associated with this instance. Must
  // outlive this instance.
  PasswordManagerClient* client_;

  // The PasswordManagerDriver instance associated with this instance. Must
  // outlive this instance.
  PasswordManagerDriver* driver_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationFrameHelper);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_FRAME_HELPER_H_
