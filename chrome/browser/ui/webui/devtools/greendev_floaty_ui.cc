// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/devtools/greendev_floaty_ui.h"

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

class FloatyMessageHandler : public content::WebUIMessageHandler {
 public:
  FloatyMessageHandler() = default;

  FloatyMessageHandler(const FloatyMessageHandler&) = delete;
  FloatyMessageHandler& operator=(const FloatyMessageHandler&) = delete;

  ~FloatyMessageHandler() override = default;

 private:
  void HandleGetNodeDescription(const base::ListValue& args) {
    AllowJavascript();
    const base::Value& callback_id = args[0];
    int process_id, routing_id, x, y;
    base::StringToInt(args[1].GetString(), &process_id);
    base::StringToInt(args[2].GetString(), &routing_id);
    base::StringToInt(args[3].GetString(), &x);
    base::StringToInt(args[4].GetString(), &y);

    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(process_id, routing_id);
    if (!rfh || !rfh->IsRenderFrameLive()) {
      ResolveJavascriptCallback(callback_id, base::Value(""));
      return;
    }

    const std::string script = base::StringPrintf(
        R"((function() {
        const element = document.elementFromPoint(%d, %d);
        if (!element) {
          return '';
        }
        let description = element.tagName.toLowerCase();
        if (element.id) {
          description += '#' + element.id;
        }
        if (element.className) {
          description += '.' + element.className.trim().replace(/\s+/g, '.');
        }
        return description;
      })())",
        x, y);

    rfh->ExecuteJavaScript(
        base::UTF8ToUTF16(script),
        base::BindOnce(&FloatyMessageHandler::OnGotNodeDescription,
                       weak_ptr_factory_.GetWeakPtr(), callback_id.Clone()));
  }

  void OnGotNodeDescription(const base::Value& callback_id,
                            base::Value result) {
    ResolveJavascriptCallback(callback_id, result);
  }

  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "getNodeDescription",
        base::BindRepeating(&FloatyMessageHandler::HandleGetNodeDescription,
                            base::Unretained(this)));
  }

  base::WeakPtrFactory<FloatyMessageHandler> weak_ptr_factory_{this};
};

}  // namespace

GreenDevFloatyUI::GreenDevFloatyUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<FloatyMessageHandler>());
}

GreenDevFloatyUI::~GreenDevFloatyUI() = default;
