// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_FROM_MIGRATE_FROM_FIELD_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_FROM_MIGRATE_FROM_FIELD_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/migration_target_install_job_result.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/scheduler/web_app_install_from_migrate_from_field_result.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace web_app {

class MigrationTargetInstallJob;
class WebAppDataRetriever;

// This command checks if any of the apps listed in the `migrate_from` field of
// the provided manifest are installed. If so, it installs the app described by
// the manifest (the "migration target") with the SUGGESTED_FROM_MIGRATION
// install source.
class WebAppInstallFromMigrateFromFieldCommand
    : public WebAppCommand<AppLock, WebAppInstallFromMigrateFromFieldResult>,
      public content::WebContentsObserver {
 public:
  WebAppInstallFromMigrateFromFieldCommand(
      base::WeakPtr<content::WebContents> web_contents,
      blink::mojom::ManifestPtr manifest,
      WebAppInstallFromMigrateFromFieldCallback callback);
  ~WebAppInstallFromMigrateFromFieldCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;

 private:
  void OnJobFinished(MigrationTargetInstallJobResult result);

  base::WeakPtr<content::WebContents> web_contents_;
  blink::mojom::ManifestPtr manifest_;

  std::unique_ptr<AppLock> lock_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<MigrationTargetInstallJob> job_;

  base::WeakPtrFactory<WebAppInstallFromMigrateFromFieldCommand> weak_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_FROM_MIGRATE_FROM_FIELD_COMMAND_H_
