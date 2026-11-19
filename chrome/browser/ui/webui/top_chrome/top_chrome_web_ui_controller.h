// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEB_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEB_UI_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace zoom {
class ZoomDisableLock;
}

namespace gfx {
class Point;
}

namespace content {
class RenderFrameHost;
class WebUI;
class WebUIDataSource;
}  // namespace content

namespace ui {
class MenuModel;
}  // namespace ui

class TopChromeWebUIController : public ui::MojoWebUIController {
 public:
  class Embedder {
   public:
    virtual void ShowUI() = 0;
    virtual void CloseUI() = 0;
    virtual void ShowContextMenu(gfx::Point point,
                                 std::unique_ptr<ui::MenuModel> menu_model) = 0;
    virtual void HideContextMenu() = 0;
  };

  // By default TopChromeWebUIController do not have normal WebUI bindings.
  // Pass |enable_chrome_send| as true if these are needed.
  explicit TopChromeWebUIController(content::WebUI* contents,
                                    bool enable_chrome_send = false,
                                    bool enable_chrome_histograms = false);
  TopChromeWebUIController(const TopChromeWebUIController&) = delete;
  TopChromeWebUIController& operator=(const TopChromeWebUIController&) = delete;
  ~TopChromeWebUIController() override;

  content::WebUIController::DisplayDisposition GetDisplayDisposition()
      const override;

  // ui::MojoWebUIController overrides:
  void WebUIPrimaryPageChanged(content::Page& page) override;
  void WebUIRenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override;

  void set_embedder(base::WeakPtr<Embedder> embedder) { embedder_ = embedder; }
  base::WeakPtr<Embedder> embedder() { return embedder_; }

  // Returns whether the WebUIDataSource of `web_ui` is shared by the windows of
  // all profiles, i.e. it is in TopChromeWebUIProfile. If so, the loadTimeData
  // values that depend on the profile must be passed to each frame with
  // AddProfileLoadTimeData() instead of being in the data source.
  static bool IsWebUIDataSourceShared(content::WebUI* web_ui);

 protected:
  // Adds `values`, the loadTimeData values that depend on the profile of the
  // window that shows this WebUI, to `source`. If the data source is shared,
  // they are passed to each frame instead, and the page must import
  // //resources/js/profile_load_time_data.js after /strings.m.js and before
  // the elements that read them. Passing them needs
  // `BindingsPolicyValue::kWebUi`, so `enable_chrome_send` must be true if the
  // data source is shared.
  void AddProfileLoadTimeData(content::WebUIDataSource* source,
                              const base::DictValue& values);

 private:
  base::WeakPtr<Embedder> embedder_;
  std::unique_ptr<zoom::ZoomDisableLock> zoom_lock_;
  // The values passed to each frame by WebUIRenderFrameCreated(), as JSON.
  std::optional<std::string> profile_load_time_data_json_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEB_UI_CONTROLLER_H_
