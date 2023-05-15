// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_HANDLER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/smb_client/smb_service.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace ash::smb_dialog {

class SmbHandler : public content::WebUIMessageHandler {
 public:
  using UpdateCredentialsCallback =
      base::OnceCallback<void(const std::string& username,
                              const std::string& password)>;

  SmbHandler(Profile* profile, UpdateCredentialsCallback update_cred_callback);

  SmbHandler(const SmbHandler&) = delete;
  SmbHandler& operator=(const SmbHandler&) = delete;

  ~SmbHandler() override;

 private:
  // content::WebUIMessageHandler
  void RegisterMessages() override;

  // WebUI call to mount an Smb Filesystem.
  void HandleSmbMount(const base::Value::List& args);

  // WebUI call to start file share discovery on the network.
  void HandleStartDiscovery(const base::Value::List& args);

  // WebUI call to update the credentials of a mounted share.
  void HandleUpdateCredentials(const base::Value::List& args);

  // Callback handler for SmbMount.
  void HandleSmbMountResponse(const std::string& callback_id,
                              smb_client::SmbMountResult result);

  // Callback handler for StartDiscovery.
  void HandleGatherSharesResponse(
      const std::vector<smb_client::SmbUrl>& shares_gathered,
      bool done);

  // Callback handler that indicates discovery is complete.
  void HandleDiscoveryDone();

  bool host_discovery_done_ = false;
  base::OnceClosure stored_mount_call_;
  const raw_ptr<Profile, ExperimentalAsh> profile_;
  UpdateCredentialsCallback update_cred_callback_;
  base::WeakPtrFactory<SmbHandler> weak_ptr_factory_{this};
};

}  // namespace ash::smb_dialog

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_HANDLER_H_
