// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UPDATE_VALIDATED_ORIGIN_ASSOCIATIONS_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UPDATE_VALIDATED_ORIGIN_ASSOCIATIONS_COMMAND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/scheduler/update_validated_origin_associations_result.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

struct OriginAssociations;

// Revalidates existing origin associations against server-side values, removing
// previously validated items that are no longer authorized. Revalidation is
// skipped when offline and rate-limited to once per 24 hours for each app.
// When requested, the immediate pending migration destination is checked
// independently, even when the source app is ineligible or throttled.
class UpdateValidatedOriginAssociationsCommand
    : public WebAppCommand<AppLock, UpdateValidatedOriginAssociationsResult> {
 public:
  UpdateValidatedOriginAssociationsCommand(
      const webapps::AppId& app_id,
      base::OnceCallback<void(UpdateValidatedOriginAssociationsResult)>
          callback,
      bool revalidate_migration_destination);
  ~UpdateValidatedOriginAssociationsCommand() override;

 protected:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void OnOriginAssociationValidated(
      OriginAssociations validated_origin_associations);

  webapps::AppId app_id_;
  const bool revalidate_migration_destination_;
  std::unique_ptr<AppLock> lock_;
  base::WeakPtrFactory<UpdateValidatedOriginAssociationsCommand> weak_factory_{
      this};
};

}  //  namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_UPDATE_VALIDATED_ORIGIN_ASSOCIATIONS_COMMAND_H_
