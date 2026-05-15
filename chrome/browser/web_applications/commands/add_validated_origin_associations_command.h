// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_ADD_VALIDATED_ORIGIN_ASSOCIATIONS_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_ADD_VALIDATED_ORIGIN_ASSOCIATIONS_COMMAND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/scheduler/add_validated_origin_associations_result.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"

namespace web_app {

// The command revalidates existing origin associations,
// effectively treating server side values as source of truth,
// meaning previously validated items might be removed, if server
// is no longer returning them.
class AddValidatedOriginAssociationsCommand
    : public WebAppCommand<AppLock, AddValidatedOriginAssociationsResult> {
 public:
  AddValidatedOriginAssociationsCommand(
      const webapps::AppId& app_id,
      base::OnceCallback<void(AddValidatedOriginAssociationsResult)> callback);
  ~AddValidatedOriginAssociationsCommand() override;

 protected:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void OnOriginAssociationValidated(
      OriginAssociations validated_origin_associations);

  webapps::AppId app_id_;
  std::unique_ptr<AppLock> lock_;
  base::WeakPtrFactory<AddValidatedOriginAssociationsCommand> weak_factory_{
      this};
};

}  //  namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_ADD_VALIDATED_ORIGIN_ASSOCIATIONS_COMMAND_H_
