// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_REQUIREMENTS_SPEC_PRINTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_REQUIREMENTS_SPEC_PRINTER_H_

#include <ostream>

#include "components/autofill/core/browser/proto/password_requirements.pb.h"

namespace autofill {

std::ostream& operator<<(
    std::ostream& out,
    const PasswordRequirementsSpec::CharacterClass& character_class);

std::ostream& operator<<(std::ostream& out,
                         const PasswordRequirementsSpec& spec);

}  // namespace autofill

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_REQUIREMENTS_SPEC_PRINTER_H_
