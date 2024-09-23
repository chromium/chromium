// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater.h"

#import <Foundation/Foundation.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/authorization_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

int GetDownloadProgress(int64_t downloaded_bytes, int64_t total_bytes) {
  if (downloaded_bytes < 0 || total_bytes <= 0)
    return -1;
  return 100 * std::clamp(static_cast<double>(downloaded_bytes) / total_bytes,
                           0.0, 1.0);
}

void UpdateStatus(VersionUpdater::StatusCallback status_callback,
                  const updater::UpdateService::UpdateState& update_state) {
  VersionUpdater::Status status = VersionUpdater::Status::CHECKING;
  int progress = 0;
  std::string version;
  std::u16string err_message;

  switch (update_state.state) {
    case updater::UpdateService::UpdateState::State::kCheckingForUpdates:
      [[fallthrough]];
    case updater::UpdateService::UpdateState::State::kUpdateAvailable:
      status = VersionUpdater::Status::CHECKING;
      break;
    case updater::UpdateService::UpdateState::State::kDownloading:
      progress = GetDownloadProgress(update_state.downloaded_bytes,
                                     update_state.total_bytes);
      [[fallthrough]];
    case updater::UpdateService::UpdateState::State::kInstalling:
      status = VersionUpdater::Status::UPDATING;
      break;
    case updater::UpdateService::UpdateState::State::kUpdated:
      status = VersionUpdater::Status::NEARLY_UPDATED;
      break;
    case updater::UpdateService::UpdateState::State::kNoUpdate:
      status = UpgradeDetector::GetInstance()->is_upgrade_available()
                   ? VersionUpdater::Status::NEARLY_UPDATED
                   : VersionUpdater::Status::UPDATED;
      break;
    case updater::UpdateService::UpdateState::State::kUpdateError:
      status = VersionUpdater::Status::FAILED;
      err_message = l10n_util::GetStringFUTF16(
          IDS_ABOUT_BOX_ERROR_DURING_UPDATE_CHECK,
          l10n_util::GetStringFUTF16(IDS_ABOUT_BOX_GOOGLE_UPDATE_ERROR,
                                     base::UTF8ToUTF16(base::StringPrintf(
                                         "%d", update_state.error_code)),
                                     base::UTF8ToUTF16(base::StringPrintf(
                                         "%d", update_state.extra_code1))));
      break;
    case updater::UpdateService::UpdateState::State::kNotStarted:
      [[fallthrough]];
    case updater::UpdateService::UpdateState::State::kUnknown:
      return;
  }

  status_callback.Run(status, progress, false, false, version, 0, err_message);
}

// macOS implementation of version update functionality, used by the WebUI
// About/Help page.
class VersionUpdaterMac : public VersionUpdater {
 public:
  VersionUpdaterMac() = default;
  VersionUpdaterMac(const VersionUpdaterMac&) = delete;
  VersionUpdaterMac& operator=(const VersionUpdaterMac&) = delete;
  ~VersionUpdaterMac() override = default;

  // VersionUpdater implementation.
  void CheckForUpdate(StatusCallback status_callback,
                      PromoteCallback promote_callback) override {
    EnsureUpdater(
        base::BindOnce(
            [](PromoteCallback prompt) {
              prompt.Run(PromotionState::PROMOTE_ENABLED);
            },
            promote_callback),
        base::BindOnce(
            [](base::RepeatingCallback<void(
                   const updater::UpdateService::UpdateState&)>
                   status_callback) {
              base::ThreadPool::PostTaskAndReplyWithResult(
                  FROM_HERE, {base::MayBlock()},
                  base::BindOnce(&GetUpdaterScope),
                  base::BindOnce(
                      [](base::RepeatingCallback<void(
                             const updater::UpdateService::UpdateState&)>
                             status_callback,
                         updater::UpdaterScope scope) {
                        BrowserUpdaterClient::Create(scope)->CheckForUpdate(
                            status_callback);
                      },
                      status_callback));
            },
            base::BindRepeating(&UpdateStatus, status_callback)));
  }
  void PromoteUpdater() override { SetupSystemUpdater(); }
};

}  // namespace

std::unique_ptr<VersionUpdater> VersionUpdater::Create(
    content::WebContents* /* web_contents */) {
  return base::WrapUnique(new VersionUpdaterMac());
}
