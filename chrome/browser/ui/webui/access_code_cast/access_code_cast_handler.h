// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_discovery_interface.h"
#include "chrome/browser/media/router/discovery/access_code/discovery_resources.pb.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

using ::media_router::AccessCodeCastDiscoveryInterface;

class AccessCodeCastHandler : public access_code_cast::mojom::PageHandler {
 public:
  using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;

  AccessCodeCastHandler(
      mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
      mojo::PendingRemote<access_code_cast::mojom::Page> page,
      Profile* profile);
  ~AccessCodeCastHandler() override;

  // access_code_cast::mojom::PageHandler overrides:
  void AddSink(const std::string& access_code,
               access_code_cast::mojom::CastDiscoveryMethod discovery_method,
               AddSinkCallback callback) override;

  // access_code_cast::mojom::PageHandler overrides:
  void CastToSink(CastToSinkCallback callback) override;

 private:
  void CreateSink(AddSinkCallback callback);

  void OnAccessCodeValidated(
      absl::optional<DiscoveryDevice> discovery_device,
      access_code_cast::mojom::AddSinkResultCode result_code);

  mojo::Remote<access_code_cast::mojom::Page> page_;
  mojo::Receiver<access_code_cast::mojom::PageHandler> receiver_;

  std::unique_ptr<AccessCodeCastDiscoveryInterface> discovery_server_interface_;

  // The dispatcher only needs to cast to the most recent sink that was
  // added. Store this value after the call to add is made.
  const std::string recent_sink_id;

  // Used to fetch OAuth2 access tokens.
  Profile* const profile_;

  AddSinkCallback add_sink_callback_;

  base::WeakPtrFactory<AccessCodeCastHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_
