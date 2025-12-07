// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_PAGE_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom-data-view.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class CommandUpdater;
class MetricsReporter;

namespace content {
class WebContents;
}  // namespace content

class ReloadButtonPageHandler : public reload_button::mojom::PageHandler {
 public:
  ReloadButtonPageHandler(
      mojo::PendingReceiver<reload_button::mojom::PageHandler> receiver,
      mojo::PendingRemote<reload_button::mojom::Page> page,
      content::WebContents* web_contents,
      CommandUpdater* command_updater);

  ReloadButtonPageHandler(const ReloadButtonPageHandler&) = delete;
  ReloadButtonPageHandler& operator=(const ReloadButtonPageHandler&) = delete;

  ~ReloadButtonPageHandler() override;

  void SetReloadButtonState(bool is_loading, bool is_menu_enabled);

  // reload_button::mojom::PageHandler:
  void Reload(bool ignore_cache,
              const std::vector<reload_button::mojom::ClickDispositionFlag>&
                  flags) override;
  void StopReload() override;
  void ShowContextMenu(int32_t offset_x, int32_t offset_y) override;

 private:
  // Returns the MetricsReporter associated with `web_contents_` or nullptr.
  //
  // This method fetches the reporter from the MetricsReporterService associated
  // with `web_contents_` each time it is called. This is necessary because the
  // MetricsReporterService lifetime is tied to `web_contents_`, which can be
  // destroyed earlier than this ReloadButtonPageHandler.
  MetricsReporter* GetMetricsReporter();

  // Callback for `MetricsReporter::Measure()`. Records the resulting
  // base::TimeDelta to the given UMA histogram and clears the start mark.
  void OnMeasureResultAndClearMark(const std::string& histogram_name,
                                   const std::string& start_mark,
                                   base::TimeDelta duration);

  mojo::Receiver<reload_button::mojom::PageHandler> receiver_;
  mojo::Remote<reload_button::mojom::Page> page_;

  // Not owned.
  const raw_ptr<content::WebContents> web_contents_;
  // Not owned.
  const raw_ptr<CommandUpdater> command_updater_;

  // Must be the last member.
  base::WeakPtrFactory<ReloadButtonPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_RELOAD_BUTTON_RELOAD_BUTTON_PAGE_HANDLER_H_
