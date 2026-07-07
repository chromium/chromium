// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/cross_device_signin_qr_bubble.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/avatar_toolbar_button_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kCrossDeviceSigninQrBubbleWebViewElementId);

namespace {
// TODO(crbug.com/527402105): Dynamically size the bubble based on the WebUI's
// preferred content size.
constexpr gfx::Size kBubbleSize(320, 420);

class CrossDeviceSigninQrWebView : public views::WebView {
 public:
  using views::WebView::WebView;

  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, GetFocusManager());
  }

 private:
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

}  // namespace

std::unique_ptr<views::BubbleDialogDelegate> CreateCrossDeviceSigninQrBubble(
    BrowserWindowInterface* browser,
    base::OnceClosure closing_callback) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(
      browser->GetBrowserForMigrationOnly());

  views::View* anchor_view = nullptr;
  if (browser_view && browser_view->toolbar()) {
    anchor_view = browser_view->toolbar()->avatar_toolbar_button();
  }

  base::ScopedClosureRunner clear_avatar_button_effects_callback;

  if (browser_view) {
    if (AvatarToolbarButtonInterface* avatar_button =
            browser_view->toolbar_button_provider()
                ->GetAvatarToolbarButtonInterface()) {
      clear_avatar_button_effects_callback =
          avatar_button->SetExplicitButtonState(
              l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SIGNIN_ON_PHONE),
              /*accessibility_label=*/std::nullopt,
              /*explicit_action=*/
              base::BindRepeating(
                  [](base::WeakPtr<Browser> weak_browser,
                     bool is_source_accelerator) {
                    if (weak_browser) {
                      weak_browser->GetFeatures()
                          .signin_view_controller()
                          ->CloseBubbleSignin();
                    }
                  },
                  browser->GetBrowserForMigrationOnly()->AsWeakPtr()));
    }
  }

  base::OnceClosure cleanup_closure = base::BindOnce(
      [](base::ScopedClosureRunner clear_effects,
         base::OnceClosure closing_callback) {
        clear_effects.RunAndReset();
        if (closing_callback) {
          std::move(closing_callback).Run();
        }
      },
      std::move(clear_avatar_button_effects_callback),
      std::move(closing_callback));

  auto web_view =
      std::make_unique<CrossDeviceSigninQrWebView>(browser->GetProfile());
  web_view->LoadInitialURL(GURL(chrome::kChromeUICrossDeviceSigninQrBubbleURL));
  web_view->SetPreferredSize(kBubbleSize);
  web_view->GetWebContents()->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_QR_CODE_BUBBLE_SIGNIN_ON_PHONE_TITLE))
          .SetDialogDestroyingCallback(std::move(cleanup_closure))
          .OverrideShowCloseButton(true)
          .DisableCloseOnDeactivate()
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::move(web_view),
                  views::BubbleDialogModelHost::FieldType::kControl),
              kCrossDeviceSigninQrBubbleWebViewElementId)
          .Build();

  auto arrow = views::BubbleBorder::TOP_RIGHT;
  if (!anchor_view) {
    // Fallback to floating the bubble in the center of the browser window if
    // the profile menu button is unavailable.
    anchor_view = browser_view;
    arrow = views::BubbleBorder::FLOAT;
  }

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, arrow);
  bubble->set_margins(gfx::Insets());

  if (browser_view && browser_view->GetWidget()) {
    bubble->set_parent_window(browser_view->GetWidget()->GetNativeView());
  }

  return bubble;
}
