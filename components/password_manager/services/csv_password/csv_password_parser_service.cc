// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/services/csv_password/csv_password_parser_service.h"

#include "components/strings/grit/components_strings.h"
#include "content/public/browser/service_process_host.h"

namespace password_manager {

mojo::Remote<password_manager::mojom::CSVPasswordParser>
LaunchCSVPasswordParser() {
  return content::ServiceProcessHost::Launch<
      password_manager::mojom::CSVPasswordParser>(
      content::ServiceProcessHost::Options()
          .WithDisplayName(
              IDS_PASSWORD_MANAGER_CSV_PASSWORD_PARSER_SERVICE_DISPLAY_NAME)
          .Pass());
}

}  // namespace password_manager
