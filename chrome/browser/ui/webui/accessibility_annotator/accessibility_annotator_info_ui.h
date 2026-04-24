
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_INFO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_INFO_UI_H_
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/webui/accessibility_annotator/accessibility_annotator_info.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
namespace content {
class BrowserContext;
}
namespace accessibility_annotator {
enum class InfoDialogResult { kAcknowledged, kDismissed };
}  // namespace accessibility_annotator
namespace accessibility_annotator::info {
class AccessibilityAnnotatorInfoPageHandler;
class AccessibilityAnnotatorInfoUI;
class AccessibilityAnnotatorInfoUIConfig
    : public DefaultTopChromeWebUIConfig<AccessibilityAnnotatorInfoUI> {
 public:
  AccessibilityAnnotatorInfoUIConfig();
  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  bool ShouldAutoResizeHost() override;
};
// WebUIController for chrome://accessibility-annotator-info
class AccessibilityAnnotatorInfoUI : public TopChromeWebUIController {
 public:
  explicit AccessibilityAnnotatorInfoUI(content::WebUI* web_ui);
  AccessibilityAnnotatorInfoUI(const AccessibilityAnnotatorInfoUI&) = delete;
  AccessibilityAnnotatorInfoUI& operator=(const AccessibilityAnnotatorInfoUI&) =
      delete;
  ~AccessibilityAnnotatorInfoUI() override;
  static constexpr std::string_view GetWebUIName() {
    return "AccessibilityAnnotatorInfo";
  }
  void BindInterface(
      mojo::PendingReceiver<accessibility_annotator::info::mojom::PageHandler>
          receiver);
  void SetDialogCallback(base::OnceCallback<void(InfoDialogResult)> callback);

  void ShowUI();

 private:
  std::unique_ptr<AccessibilityAnnotatorInfoPageHandler> page_handler_;
  base::OnceCallback<void(InfoDialogResult)> dialog_callback_ =
      base::DoNothing();
  WEB_UI_CONTROLLER_TYPE_DECL();
};
}  // namespace accessibility_annotator::info
#endif  // CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_ACCESSIBILITY_ANNOTATOR_INFO_UI_H_
