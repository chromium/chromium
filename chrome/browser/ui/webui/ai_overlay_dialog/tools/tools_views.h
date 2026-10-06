// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_TOOLS_TOOLS_VIEWS_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_TOOLS_TOOLS_VIEWS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.h"

namespace ttc {

// Desktop (Views) implementation of AiOverlayTools.
class AiOverlayToolsViews : public AiOverlayTools {
 public:
  // Use AiOverlayTools::Create() instead of constructing this directly.
  AiOverlayToolsViews(
      base::PassKey<AiOverlayTools>,
      mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver,
      BrowserWindowInterface* browser,
      PageContextMonitor* page_context_monitor);
  AiOverlayToolsViews(const AiOverlayToolsViews&) = delete;
  AiOverlayToolsViews& operator=(const AiOverlayToolsViews&) = delete;
  ~AiOverlayToolsViews() override;

  // ai_overlay_dialog::mojom::AiOverlayTools:
  void SwitchTab(const std::string& query, SwitchTabCallback callback) override;
  void CloseCurrentTab(CloseCurrentTabCallback callback) override;
  void Scroll(ai_overlay_dialog::mojom::ScrollGranularity granularity,
              double magnitude,
              ScrollCallback callback) override;
  void OpenPage(const std::string& query, OpenPageCallback callback) override;
  void SetFullscreen(bool fullscreen, SetFullscreenCallback callback) override;
  void OpenGeminiPanel(const std::string& prompt,
                       OpenGeminiPanelCallback callback) override;
  void CloseGeminiPanel(CloseGeminiPanelCallback callback) override;

 private:
  base::WeakPtrFactory<AiOverlayToolsViews> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_TOOLS_TOOLS_VIEWS_H_
