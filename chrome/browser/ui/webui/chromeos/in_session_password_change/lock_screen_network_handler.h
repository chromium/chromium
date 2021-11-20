
// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_NETWORK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_NETWORK_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

class NetworkConfigMessageHandler : public content::WebUIMessageHandler {
 public:
  explicit NetworkConfigMessageHandler();
  ~NetworkConfigMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void Initialize(base::Value::ConstListView args);
  void ShowNetworkDetails(base::Value::ConstListView args);
  void ShowNetworkConfig(base::Value::ConstListView args);
  void AddNetwork(base::Value::ConstListView args);
  void GetHostname(base::Value::ConstListView args);
  void Respond(const std::string& callback_id, const base::Value& response);

  base::WeakPtrFactory<NetworkConfigMessageHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_IN_SESSION_PASSWORD_CHANGE_LOCK_SCREEN_NETWORK_HANDLER_H_
