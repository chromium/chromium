// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/util_win_service.h"

#include "chrome/grit/generated_resources.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "content/public/browser/service_process_host.h"

mojo::Remote<chrome::mojom::UtilWin> LaunchUtilWinServiceInstance() {
  // Runs with kNoSandbox from sandbox.mojom.Sandbox.
  return content::ServiceProcessHost::Launch<chrome::mojom::UtilWin>(
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_UTILITY_WIN_NAME)
          .Pass());
}

mojo::Remote<chrome::mojom::ProcessorMetrics> LaunchProcessorMetricsService() {
  // Runs with kNoSandbox from sandbox.mojom.Sandbox.
  return content::ServiceProcessHost::Launch<chrome::mojom::ProcessorMetrics>(
      content::ServiceProcessHost::Options()
          .WithDisplayName(IDS_UTILITY_PROCESS_UTILITY_WIN_NAME)
          .Pass());
}
