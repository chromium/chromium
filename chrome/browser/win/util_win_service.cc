// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/util_win_service.h"

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/sandbox_type.h"
#include "content/public/browser/service_process_host.h"

mojo::Remote<chrome::mojom::UtilWin> LaunchUtilWinServiceInstance() {
  return content::ServiceProcessHost::Launch<chrome::mojom::UtilWin>(
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_UTILITY_WIN_NAME)
          .WithSandboxType(service_manager::SANDBOX_TYPE_NO_SANDBOX)
          .Pass());
}
