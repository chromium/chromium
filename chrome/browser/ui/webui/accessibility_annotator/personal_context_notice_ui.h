
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_UI_H_
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/webui/accessibility_annotator/personal_context_notice.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
namespace content {
class BrowserContext;
}
namespace personal_context {
enum class NoticeDialogResult { kAcknowledged, kDismissed };
}  // namespace personal_context
namespace personal_context::notice {
class PersonalContextNoticePageHandler;
class PersonalContextNoticeUI;
class PersonalContextNoticeUIConfig
    : public DefaultTopChromeWebUIConfig<PersonalContextNoticeUI> {
 public:
  PersonalContextNoticeUIConfig();
  // DefaultTopChromeWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  bool ShouldAutoResizeHost() override;
};
// WebUIController for chrome://accessibility-annotator-info
class PersonalContextNoticeUI : public TopChromeWebUIController {
 public:
  explicit PersonalContextNoticeUI(content::WebUI* web_ui);
  PersonalContextNoticeUI(const PersonalContextNoticeUI&) = delete;
  PersonalContextNoticeUI& operator=(const PersonalContextNoticeUI&) = delete;
  ~PersonalContextNoticeUI() override;
  static constexpr std::string_view GetWebUIName() {
    return "AccessibilityAnnotatorInfo";
  }
  void BindInterface(
      mojo::PendingReceiver<personal_context::notice::mojom::PageHandler>
          receiver);
  void SetDialogCallback(base::OnceCallback<void(NoticeDialogResult)> callback);

  void ShowUI();

 private:
  std::unique_ptr<PersonalContextNoticePageHandler> page_handler_;
  base::OnceCallback<void(NoticeDialogResult)> dialog_callback_ =
      base::DoNothing();
  WEB_UI_CONTROLLER_TYPE_DECL();
};
}  // namespace personal_context::notice
#endif  // CHROME_BROWSER_UI_WEBUI_ACCESSIBILITY_ANNOTATOR_PERSONAL_CONTEXT_NOTICE_UI_H_
