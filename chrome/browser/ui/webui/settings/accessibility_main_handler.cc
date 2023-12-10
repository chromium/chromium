// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/accessibility_labels_bubble_model.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#include "ui/accessibility/accessibility_features.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

namespace settings {

AccessibilityMainHandler::AccessibilityMainHandler() = default;

AccessibilityMainHandler::~AccessibilityMainHandler() = default;

void AccessibilityMainHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "a11yPageReady",
      base::BindRepeating(&AccessibilityMainHandler::HandleA11yPageReady,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "confirmA11yImageLabels",
      base::BindRepeating(
          &AccessibilityMainHandler::HandleCheckAccessibilityImageLabels,
          base::Unretained(this)));
}

void AccessibilityMainHandler::OnJavascriptAllowed() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  accessibility_subscription_ =
      ash::AccessibilityManager::Get()->RegisterCallback(base::BindRepeating(
          &AccessibilityMainHandler::OnAccessibilityStatusChanged,
          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  if (features::IsPdfOcrEnabled()) {
    CHECK(!component_ready_observer_.IsObserving());
    component_ready_observer_.Observe(
        screen_ai::ScreenAIInstallState::GetInstance());
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
}

void AccessibilityMainHandler::OnJavascriptDisallowed() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  accessibility_subscription_ = {};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  if (features::IsPdfOcrEnabled()) {
    component_ready_observer_.Reset();
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
void AccessibilityMainHandler::DownloadProgressChanged(double progress) {
  CHECK_GE(progress, 0.0);
  CHECK_LE(progress, 1.0);
  const int progress_num = progress * 100;
  FireWebUIListener("pdf-ocr-downloading-progress-changed",
                    base::Value(progress_num));
}

void AccessibilityMainHandler::StateChanged(
    screen_ai::ScreenAIInstallState::State state) {
  base::Value state_value = base::Value(static_cast<int>(state));
  FireWebUIListener("pdf-ocr-state-changed", state_value);
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

void AccessibilityMainHandler::HandleA11yPageReady(
    const base::Value::List& args) {
  AllowJavascript();
  SendScreenReaderStateChanged();
}

void AccessibilityMainHandler::HandleCheckAccessibilityImageLabels(
    const base::Value::List& args) {
  // When the user tries to enable the feature, show the modal dialog. The
  // dialog will disable the feature again if it is not accepted.
  content::WebContents* web_contents = web_ui()->GetWebContents();
  content::RenderWidgetHostView* view = web_contents->GetPrimaryMainFrame()
                                            ->GetRenderViewHost()
                                            ->GetWidget()
                                            ->GetView();
  gfx::Rect rect = view->GetViewBounds();
  auto model = std::make_unique<AccessibilityLabelsBubbleModel>(
      Profile::FromWebUI(web_ui()), web_contents, true /* enable always */);
  chrome::ShowConfirmBubble(
      web_contents->GetTopLevelNativeWindow(), view->GetNativeView(),
      gfx::Point(rect.CenterPoint().x(), rect.y()), std::move(model));
}

void AccessibilityMainHandler::SendScreenReaderStateChanged() {
  base::Value result(accessibility_state_utils::IsScreenReaderEnabled());
  FireWebUIListener("screen-reader-state-changed", result);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AccessibilityMainHandler::OnAccessibilityStatusChanged(
    const ash::AccessibilityStatusEventDetails& details) {
  if (details.notification_type ==
      ash::AccessibilityNotificationType::kToggleSpokenFeedback) {
    SendScreenReaderStateChanged();
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace settings
