// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools_android.h"

#include <string>
#include <variant>

#include "base/types/expected.h"
#include "url/gurl.h"

namespace ttc {

// static
std::unique_ptr<AiOverlayTools> AiOverlayTools::Create(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver,
    BrowserWindowInterface* browser,
    PageContextMonitor* page_context_monitor) {
  return std::make_unique<AiOverlayToolsAndroid>(
      base::PassKey<AiOverlayTools>(), std::move(receiver), browser,
      page_context_monitor);
}

AiOverlayToolsAndroid::AiOverlayToolsAndroid(
    base::PassKey<AiOverlayTools>,
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver,
    BrowserWindowInterface* browser,
    PageContextMonitor* page_context_monitor)
    : AiOverlayTools(std::move(receiver), browser, page_context_monitor) {}

AiOverlayToolsAndroid::~AiOverlayToolsAndroid() = default;

content::WebContents* AiOverlayToolsAndroid::GetActiveWebContents() const {
  return nullptr;
}

void AiOverlayToolsAndroid::OpenUrl(const std::string& url_string,
                                    bool new_tab,
                                    OpenUrlCallback callback) {
  RecordToolCallInvoked("OpenUrl");
  GURL url(url_string);
  if (!url.is_valid()) {
    std::move(callback).Run(base::unexpected("Invalid URL"));
    return;
  }

  // TODO(crbug.com/540589868): Support asynchronous URL navigation on Android.
  std::move(callback).Run(base::unexpected("OpenUrl not yet ported to Clank"));
}

void AiOverlayToolsAndroid::SwitchTab(const std::string& query,
                                      SwitchTabCallback callback) {
  RecordToolCallInvoked("SwitchTab");
  std::move(callback).Run(
      base::unexpected("SwitchTab not yet ported to Clank"));
}

void AiOverlayToolsAndroid::CloseCurrentTab(CloseCurrentTabCallback callback) {
  RecordToolCallInvoked("CloseCurrentTab");
  std::move(callback).Run(
      base::unexpected("CloseCurrentTab not yet ported to Clank"));
}

void AiOverlayToolsAndroid::Scroll(
    ai_overlay_dialog::mojom::ScrollGranularity granularity,
    double magnitude,
    ScrollCallback callback) {
  RecordToolCallInvoked("Scroll");
  std::move(callback).Run(std::monostate());
}

void AiOverlayToolsAndroid::OpenPage(const std::string& query,
                                     OpenPageCallback callback) {
  RecordToolCallInvoked("OpenPage");
  std::move(callback).Run(base::unexpected("OpenPage not yet ported to Clank"));
}

void AiOverlayToolsAndroid::SetFullscreen(bool fullscreen,
                                          SetFullscreenCallback callback) {
  RecordToolCallInvoked("SetFullscreen");
  std::move(callback).Run(
      base::unexpected("SetFullscreen not yet ported to Clank"));
}

void AiOverlayToolsAndroid::OpenGeminiPanel(const std::string& prompt,
                                            OpenGeminiPanelCallback callback) {
  RecordToolCallInvoked("OpenGeminiPanel");
  std::move(callback).Run(base::unexpected("Glic service not available"));
}

void AiOverlayToolsAndroid::CloseGeminiPanel(
    CloseGeminiPanelCallback callback) {
  RecordToolCallInvoked("CloseGeminiPanel");
  std::move(callback).Run(base::unexpected("Glic service not available"));
}

}  // namespace ttc
