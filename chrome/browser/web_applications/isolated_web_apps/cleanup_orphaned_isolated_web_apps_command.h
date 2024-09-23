// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CLEANUP_ORPHANED_ISOLATED_WEB_APPS_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CLEANUP_ORPHANED_ISOLATED_WEB_APPS_COMMAND_H_

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"

class Profile;

namespace base {
class FilePath;
}

namespace web_app {

struct CleanupOrphanedIsolatedWebAppsCommandSuccess {
  explicit CleanupOrphanedIsolatedWebAppsCommandSuccess(
      int number_of_cleaned_up_directories);
  CleanupOrphanedIsolatedWebAppsCommandSuccess(
      const CleanupOrphanedIsolatedWebAppsCommandSuccess& other);
  ~CleanupOrphanedIsolatedWebAppsCommandSuccess();

  int number_of_cleaned_up_directories;
};

std::ostream& operator<<(
    std::ostream& os,
    const CleanupOrphanedIsolatedWebAppsCommandSuccess& success);

struct CleanupOrphanedIsolatedWebAppsCommandError {
  enum class Type { kCouldNotDeleteAllBundles, kSystemShutdown };

  Type type;
  std::string message;
};

std::ostream& operator<<(
    std::ostream& os,
    const CleanupOrphanedIsolatedWebAppsCommandError& error);

class CleanupOrphanedIsolatedWebAppsCommand
    : public WebAppCommand<
          AllAppsLock,
          base::expected<CleanupOrphanedIsolatedWebAppsCommandSuccess,
                         CleanupOrphanedIsolatedWebAppsCommandError>> {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<CleanupOrphanedIsolatedWebAppsCommandSuccess,
                     CleanupOrphanedIsolatedWebAppsCommandError>)>;

  CleanupOrphanedIsolatedWebAppsCommand(Profile& profile, Callback callback);
  CleanupOrphanedIsolatedWebAppsCommand(
      const CleanupOrphanedIsolatedWebAppsCommand&) = delete;
  CleanupOrphanedIsolatedWebAppsCommand& operator=(
      const CleanupOrphanedIsolatedWebAppsCommand&) = delete;
  CleanupOrphanedIsolatedWebAppsCommand(
      CleanupOrphanedIsolatedWebAppsCommand&&) = delete;
  CleanupOrphanedIsolatedWebAppsCommand& operator=(
      CleanupOrphanedIsolatedWebAppsCommand&&) = delete;

  ~CleanupOrphanedIsolatedWebAppsCommand() override;

  // This enum represents every error type that can occur during the delete
  // operation of the orphaned isolated web apps.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CleanupOrphanedIWAsUMAError {
    kCantDeleteAllOrphanedApps = 1,
    kSystemShutdown = 2,
    kMaxValue = kSystemShutdown
  };

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;

 private:
  void OnIsolatedWebAppsDirectoriesRetrieved(
      std::set<base::FilePath> isolated_web_apps_directories);
  void CommandComplete(bool success);

  int number_of_deleted_directories_ = 0;

  std::unique_ptr<AllAppsLock> lock_;

  raw_ref<Profile> profile_;
  base::WeakPtrFactory<CleanupOrphanedIsolatedWebAppsCommand> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CLEANUP_ORPHANED_ISOLATED_WEB_APPS_COMMAND_H_
