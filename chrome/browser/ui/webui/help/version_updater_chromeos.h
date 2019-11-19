// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/help/version_updater.h"

namespace content {
class BrowserContext;
class WebContents;
}

class VersionUpdaterCros : public VersionUpdater,
                           public chromeos::UpdateEngineClient::Observer {
 public:
  // VersionUpdater implementation.
  void CheckForUpdate(const StatusCallback& callback,
                      const PromoteCallback&) override;
  void SetChannel(const std::string& channel,
                  bool is_powerwash_allowed) override;
  void GetChannel(bool get_current_channel,
                  const ChannelCallback& callback) override;
  void GetEolInfo(EolInfoCallback callback) override;
  void SetUpdateOverCellularOneTimePermission(const StatusCallback& callback,
                                              const std::string& update_version,
                                              int64_t update_size) override;

  // Gets the last update status, without triggering a new check or download.
  void GetUpdateStatus(const StatusCallback& callback);

 protected:
  friend class VersionUpdater;

  // Clients must use VersionUpdater::Create().
  explicit VersionUpdaterCros(content::WebContents* web_contents);
  ~VersionUpdaterCros() override;

 private:
  // UpdateEngineClient::Observer implementation.
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

  // Callback from UpdateEngineClient::RequestUpdateCheck().
  void OnUpdateCheck(chromeos::UpdateEngineClient::UpdateCheckResult result);

  // Callback from UpdateEngineClient::SetUpdateOverCellularOneTimePermission().
  void OnSetUpdateOverCellularOneTimePermission(bool success);

  // Callback from UpdateEngineClient::GetChannel().
  void OnGetChannel(const ChannelCallback& cb,
                    const std::string& current_channel);

  // Callback from UpdateEngineClient::GetEolInfo().
  void OnGetEolInfo(EolInfoCallback cb,
                    chromeos::UpdateEngineClient::EolInfo eol_info);

  // BrowserContext in which the class was instantiated.
  content::BrowserContext* context_;

  // Callback used to communicate update status to the client.
  StatusCallback callback_;

  // Last state received via UpdateStatusChanged().
  update_engine::Operation last_operation_;

  // True if an update check should be scheduled when the update engine is idle.
  bool check_for_update_when_idle_;

  base::WeakPtrFactory<VersionUpdaterCros> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterCros);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_
