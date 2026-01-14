// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_PAGE_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"

class CommandUpdater;
class MetricsReporter;

namespace content {
class WebContents;
}  // namespace content

class WebUIToolbarPageHandler : public webui_toolbar::mojom::PageHandler {
 public:
  class WebUIToolbarDelegate {
   public:
    virtual void HandleContextMenu(
        webui_toolbar::mojom::ContextMenuType menu_type,
        gfx::Point viewport_coordinate_css_pixels,
        ui::mojom::MenuSourceType source) = 0;
  };

  WebUIToolbarPageHandler(
      mojo::PendingReceiver<webui_toolbar::mojom::PageHandler> receiver,
      mojo::PendingRemote<webui_toolbar::mojom::Page> page,
      content::WebContents* web_contents,
      CommandUpdater* command_updater,
      WebUIToolbarDelegate* web_view);

  WebUIToolbarPageHandler(const WebUIToolbarPageHandler&) = delete;
  WebUIToolbarPageHandler& operator=(const WebUIToolbarPageHandler&) = delete;

  ~WebUIToolbarPageHandler() override;

  void SetReloadButtonState(bool is_loading, bool is_menu_enabled);

  // webui_toolbar::mojom::PageHandler:
  void Reload(bool ignore_cache,
              const std::vector<webui_toolbar::mojom::ClickDispositionFlag>&
                  flags) override;
  void StopReload() override;
  void ShowContextMenu(webui_toolbar::mojom::ContextMenuType menu_type,
                       const gfx::Point& viewport_coordinate_css_pixels,
                       ui::mojom::MenuSourceType source) override;

 private:
  // Returns the MetricsReporter associated with `web_contents_` or nullptr.
  //
  // This method fetches the reporter from the MetricsReporterService associated
  // with `web_contents_` each time it is called. This is necessary because the
  // MetricsReporterService lifetime is tied to `web_contents_`, which can be
  // destroyed earlier than this WebUIToolbarPageHandler.
  MetricsReporter* GetMetricsReporter();

  // Callback for `MetricsReporter::Measure()`. Records the resulting
  // base::TimeDelta to the given UMA histogram and clears the start mark.
  void OnMeasureResultAndClearMark(const std::string& histogram_name,
                                   const std::string& start_mark,
                                   base::TimeDelta duration);

  mojo::Receiver<webui_toolbar::mojom::PageHandler> receiver_;
  mojo::Remote<webui_toolbar::mojom::Page> page_;

  // Not owned.
  const raw_ptr<content::WebContents> web_contents_;
  // Not owned.
  const raw_ptr<CommandUpdater> command_updater_;

  raw_ptr<WebUIToolbarDelegate> delegate_;

  // Must be the last member.
  base::WeakPtrFactory<WebUIToolbarPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_PAGE_HANDLER_H_
