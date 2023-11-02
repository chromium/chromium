// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"

#include <utility>

#include "base/bind.h"
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
}

void AccessibilityMainHandler::OnJavascriptDisallowed() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  accessibility_subscription_ = {};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

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
