// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBUI_CAST_WEBUI_H_
#define CHROMECAST_BROWSER_WEBUI_CAST_WEBUI_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromecast/browser/webui/mojom/webui.mojom.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
class WebContents;
class WebUI;
}  // namespace content

namespace chromecast {

class CastWebUIMessageHandler;

class CastWebUI : public mojom::WebUi, public content::WebUIController {
 public:
  CastWebUI(content::WebUI* webui,
            const std::string& host,
            mojom::WebUiClient* client);
  ~CastWebUI() override;

  static std::unique_ptr<CastWebUI> Create(content::WebUI* webui,
                                           const std::string& host,
                                           mojom::WebUiClient* client);

 protected:
  content::WebContents* const web_contents_;
  content::BrowserContext* const browser_context_;

 private:
  void InvokeCallback(const std::string& message,
                      const base::Value::List& args);

  // mojom::WebUI implementation:
  void RegisterMessageCallback(
      const std::string& message,
      mojo::PendingRemote<mojom::MessageCallback> callback) override;
  void CallJavascriptFunction(const std::string& function,
                              base::Value::List args) override;

  // Pointer to the generic message handler owned by the Web UI. The message
  // handler is guaranteed to outlive CastWebUI since |this| is the first member
  // to be deleted in the Web UI.
  CastWebUIMessageHandler* message_handler_;

  mojo::Receiver<mojom::WebUi> web_ui_{this};

  base::flat_map<std::string, mojo::Remote<mojom::MessageCallback>>
      message_callbacks_;

  base::WeakPtr<CastWebUI> weak_this_;
  base::WeakPtrFactory<CastWebUI> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBUI_CAST_WEBUI_H_
