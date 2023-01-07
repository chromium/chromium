// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_UI_H_

#include "base/time/time.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace media_router {
// The WebUI controller for chrome://access-code-cast.
class AccessCodeCastUI : public ui::MojoWebDialogUI,
                         public access_code_cast::mojom::PageHandlerFactory {
 public:
  explicit AccessCodeCastUI(content::WebUI* web_ui);
  ~AccessCodeCastUI() override;

  AccessCodeCastUI(const AccessCodeCastUI&) = delete;
  AccessCodeCastUI& operator=(const AccessCodeCastUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<access_code_cast::mojom::PageHandlerFactory>
          receiver);

  // Set the set of modes that should be attempted when casting.
  virtual void SetCastModeSet(const media_router::CastModeSet& cast_mode_set);

  virtual void SetDialogCreationTimestamp(base::Time dialog_creation_timestamp);

  virtual void SetMediaRouteStarter(
      std::unique_ptr<media_router::MediaRouteStarter> media_route_starter);

 private:
  // access_code_cast::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<access_code_cast::mojom::Page> page,
      mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler)
      override;

  std::unique_ptr<media_router::AccessCodeCastHandler> page_handler_;
  mojo::Receiver<access_code_cast::mojom::PageHandlerFactory> factory_receiver_{
      this};

  media_router::CastModeSet cast_mode_set_;
  std::unique_ptr<media_router::MediaRouteStarter> media_route_starter_;
  absl::optional<base::Time> dialog_creation_timestamp_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_UI_H_
