// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_HANDLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/unexportable_keys_internals/unexportable_keys_internals.mojom.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace unexportable_keys {
class UnexportableKeyService;
}  // namespace unexportable_keys

class UnexportableKeysInternalsHandler
    : public unexportable_keys_internals::mojom::PageHandler {
 public:
  // Initializes the handler with the mojo handlers and the needed information
  // to be displayed as well as callbacks to the main native view.
  UnexportableKeysInternalsHandler(
      mojo::PendingReceiver<unexportable_keys_internals::mojom::PageHandler>
          receiver,
      mojo::PendingRemote<unexportable_keys_internals::mojom::Page> page,
      std::unique_ptr<unexportable_keys::UnexportableKeyService> key_service);
  ~UnexportableKeysInternalsHandler() override;

  UnexportableKeysInternalsHandler(const UnexportableKeysInternalsHandler&) =
      delete;
  UnexportableKeysInternalsHandler& operator=(
      const UnexportableKeysInternalsHandler&) = delete;

  // unexportable_keys_internals::mojom::PageHandler:
  void GetUnexportableKeysInfo(
      GetUnexportableKeysInfoCallback callback) override;
  void DeleteKey(const unexportable_keys::UnexportableKeyId& key_id,
                 DeleteKeyCallback callback) override;

 private:
  void OnGetAllSigningKeysForGarbageCollection(
      GetUnexportableKeysInfoCallback callback,
      unexportable_keys::ServiceErrorOr<
          std::vector<unexportable_keys::UnexportableKeyId>> keys);

  // Allows handling received messages from the web ui page.
  mojo::Receiver<unexportable_keys_internals::mojom::PageHandler> receiver_;
  // Interface to send information to the web ui page.
  mojo::Remote<unexportable_keys_internals::mojom::Page> page_;
  std::unique_ptr<unexportable_keys::UnexportableKeyService> key_service_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_HANDLER_H_
