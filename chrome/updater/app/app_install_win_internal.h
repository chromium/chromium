// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_INSTALL_WIN_INTERNAL_H_
#define CHROME_UPDATER_APP_APP_INSTALL_WIN_INTERNAL_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/update_service.h"

namespace base {
class Time;
class TimeDelta;
class Version;
}  // namespace base

namespace updater {

// Forwards `AppInstallProgress` calls to a target which can be replaced while
// an install is in progress. The most recent call is remembered and replayed
// to a new target, so that a UI which takes over mid-install, or after the
// install has completed, shows the current state instead of its initial state.
//
// All methods must be called on the sequence which created this object.
class AppInstallProgressForwarder : public AppInstallProgress {
 public:
  AppInstallProgressForwarder();
  AppInstallProgressForwarder(const AppInstallProgressForwarder&) = delete;
  AppInstallProgressForwarder& operator=(const AppInstallProgressForwarder&) =
      delete;
  ~AppInstallProgressForwarder() override;

  // Sets the target of the forwarded calls and replays the most recent call,
  // if any, to it. The target is not owned; it must outlive this object or be
  // replaced before it is destroyed. A null target drops the calls.
  void SetTarget(AppInstallProgress* target);

  // Overrides for AppInstallProgress.
  void OnCheckingForUpdate() override;
  void OnUpdateAvailable(const std::string& app_id,
                         const std::u16string& app_name,
                         const base::Version& version) override;
  void OnWaitingToDownload(const std::string& app_id,
                           const std::u16string& app_name) override;
  void OnDownloading(const std::string& app_id,
                     const std::u16string& app_name,
                     std::optional<base::TimeDelta> time_remaining,
                     int pos) override;
  void OnWaitingRetryDownload(const std::string& app_id,
                              const std::u16string& app_name,
                              base::Time next_retry_time) override;
  void OnWaitingToInstall(const std::string& app_id,
                          const std::u16string& app_name) override;
  void OnInstalling(const std::string& app_id,
                    const std::u16string& app_name,
                    std::optional<base::TimeDelta> time_remaining,
                    int pos) override;
  void OnPause() override;
  void OnComplete(const ObserverCompletionInfo& observer_info) override;

 private:
  // Runs `call` on the current target, if any, and remembers it for replay.
  void Forward(base::RepeatingCallback<void(AppInstallProgress*)> call);

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<AppInstallProgress> target_ = nullptr;
  base::RepeatingCallback<void(AppInstallProgress*)> last_call_;
};

[[nodiscard]] ObserverCompletionInfo HandleInstallResult(
    const UpdateService::UpdateState& update_state,
    const std::wstring& lang);

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_INSTALL_WIN_INTERNAL_H_
