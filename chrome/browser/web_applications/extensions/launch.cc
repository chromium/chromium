// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/launch.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"

namespace web_app {

void LaunchExtensionOrWebApp(
    Profile* profile,
    apps::AppLaunchParams params,
    base::OnceCallback<void(content::WebContents*)> callback) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          params.app_id);
  if (extension) {
    std::move(callback).Run(::OpenApplication(profile, std::move(params)));
    return;
  }

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);
  provider->scheduler().LaunchAppWithCustomParams(
      std::move(params),
      base::BindOnce(
          [](base::OnceCallback<void(content::WebContents*)> callback,
             base::WeakPtr<Browser> browser,
             base::WeakPtr<content::WebContents> web_contents,
             apps::LaunchContainer launch_container) {
            std::move(callback).Run(web_contents.get());
          },
          std::move(callback)));
}

}  // namespace web_app
