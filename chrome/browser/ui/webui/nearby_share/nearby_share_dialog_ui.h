// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_NEARBY_SHARE_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_NEARBY_SHARE_DIALOG_UI_H_

#include <memory>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class NearbySharingService;

namespace nearby_share {

// The WebUI controller for chrome://nearby.
class NearbyShareDialogUI : public ui::MojoWebUIController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnClose() = 0;
  };

  explicit NearbyShareDialogUI(content::WebUI* web_ui);
  NearbyShareDialogUI(const NearbyShareDialogUI&) = delete;
  NearbyShareDialogUI& operator=(const NearbyShareDialogUI&) = delete;
  ~NearbyShareDialogUI() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void SetAttachments(std::vector<std::unique_ptr<Attachment>> attachments);

  // Instantiates the implementor of the mojom::DiscoveryManager mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<mojom::DiscoveryManager> manager);
  // ui::MojoWebUIController
  void BindInterface(
      mojo::PendingReceiver<mojom::NearbyShareSettings> receiver);
  // Binds to the existing contacts manager instance owned by the nearby share
  // keyed service.
  void BindInterface(
      mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver);

 private:
  void HandleClose(const base::ListValue* args);
  std::vector<std::unique_ptr<Attachment>> attachments_;
  base::ObserverList<Observer> observers_;
  NearbySharingService* nearby_service_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace nearby_share

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_NEARBY_SHARE_DIALOG_UI_H_
