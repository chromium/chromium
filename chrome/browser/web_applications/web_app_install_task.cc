// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_task.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

// static
std::unique_ptr<content::WebContents> WebAppInstallTask::CreateWebContents(
    Profile* profile) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));

  CreateWebAppInstallTabHelpers(web_contents.get());

  return web_contents;
}

}  // namespace web_app
