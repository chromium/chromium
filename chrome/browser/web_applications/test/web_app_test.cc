// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/components/web_application_info.h"

namespace web_app {

void TestAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    InstallManager::WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback), true /*accept*/,
                                std::move(web_app_info)));
}

void TestDeclineDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    InstallManager::WebAppInstallationAcceptanceCallback acceptance_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(acceptance_callback),
                                false /*accept*/, std::move(web_app_info)));
}

}  // namespace web_app
