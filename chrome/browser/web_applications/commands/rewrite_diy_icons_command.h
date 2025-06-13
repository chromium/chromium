// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_REWRITE_DIY_ICONS_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_REWRITE_DIY_ICONS_COMMAND_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

struct ShortcutInfo;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with RewriteIconResult
// in tools/metrics/histograms/metadata/webapps/enums.xml.
enum class RewriteIconResult {
  kUnexpectedAppStateChange = 0,
  kUpdateSucceeded = 1,
  kShortcutInfoFetchFailed = 2,
  kUpdateShortcutFailed = 3,
  kMaxValue = kUpdateShortcutFailed
};

class RewriteDiyIconsCommand
    : public WebAppCommand<AppLock, RewriteIconResult> {
 public:
  RewriteDiyIconsCommand(const webapps::AppId& app_id,
                         base::OnceCallback<void(RewriteIconResult)> callback);

  ~RewriteDiyIconsCommand() override;

  RewriteDiyIconsCommand(const RewriteDiyIconsCommand&) = delete;
  RewriteDiyIconsCommand& operator=(const RewriteDiyIconsCommand&) = delete;

  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void OnGetShortcutInfo(std::unique_ptr<ShortcutInfo> info);
  void OnUpdatedShortcuts(bool success);

  const webapps::AppId app_id_;
  std::unique_ptr<AppLock> lock_;
  base::WeakPtrFactory<RewriteDiyIconsCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_REWRITE_DIY_ICONS_COMMAND_H_
