// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_HANDLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/smb_client/smb_service.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace chromeos {
namespace smb_dialog {

using smb_client::SmbMountResult;

class SmbHandler : public content::WebUIMessageHandler {
 public:
  explicit SmbHandler(Profile* profile);
  ~SmbHandler() override;

 private:
  // content::WebUIMessageHandler
  void RegisterMessages() override;

  // WebUI call to mount an Smb Filesystem.
  void HandleSmbMount(const base::ListValue* args);

  // WebUI call to start file share discovery on the network.
  void HandleStartDiscovery(const base::ListValue* args);

  // WebUI call to update the credentials of a mounted share.
  void HandleUpdateCredentials(const base::ListValue* args);

  // Callback handler for SmbMount.
  void HandleSmbMountResponse(const std::string& callback_id,
                              SmbMountResult result);

  // Callback handler for StartDiscovery.
  void HandleGatherSharesResponse(
      const std::vector<smb_client::SmbUrl>& shares_gathered,
      bool done);

  // Callback handler that indicates discovery is complete.
  void HandleDiscoveryDone();

  bool host_discovery_done_ = false;
  base::OnceClosure stored_mount_call_;
  Profile* const profile_;
  base::WeakPtrFactory<SmbHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SmbHandler);
};

}  // namespace smb_dialog
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_SMB_SHARES_SMB_HANDLER_H_
