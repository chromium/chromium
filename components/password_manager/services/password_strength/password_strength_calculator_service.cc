// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/password_strength/password_strength_calculator_service.h"

#include "components/strings/grit/components_strings.h"
#include "content/public/browser/service_process_host.h"

namespace password_manager {

mojo::Remote<password_manager::mojom::PasswordStrengthCalculator>
LaunchPasswordStrengthCalculator() {
  return content::ServiceProcessHost::Launch<
      password_manager::mojom::PasswordStrengthCalculator>(
      content::ServiceProcessHost::Options()
          .WithDisplayName(
              IDS_PASSWORD_MANAGER_PASSWORD_STRENGTH_CALCULATOR_SERVICE_DISPLAY_NAME)
          .Pass());
}

}  // namespace password_manager
