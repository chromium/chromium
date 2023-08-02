// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/ash/mako/mako_source.h"
#include "chrome/browser/ui/webui/ash/mako/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/webui/untrusted_bubble_web_ui_controller.h"

namespace {
constexpr gfx::Size kExtensionWindowSize(420, 480);
constexpr int kPaddingAroundCursor = 8;

class MakoDialogView : public WebUIBubbleDialogView {
 public:
  explicit MakoDialogView(
      std::unique_ptr<BubbleContentsWrapper> contents_wrapper)
      : WebUIBubbleDialogView(nullptr, contents_wrapper.get()),
        contents_wrapper_(std::move(contents_wrapper)) {
    set_has_parent(false);
    set_corner_radius(20);
  }

 private:
  std::unique_ptr<BubbleContentsWrapper> contents_wrapper_;
};

}  // namespace

namespace ash {

MakoUntrustedUIConfig::MakoUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme, ash::kChromeUIMakoHost) {}

MakoUntrustedUIConfig::~MakoUntrustedUIConfig() = default;

std::unique_ptr<content::WebUIController>
MakoUntrustedUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                             const GURL& url) {
  return std::make_unique<MakoUntrustedUI>(web_ui);
}

bool MakoUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kOrca);
}

MakoUntrustedUI::MakoUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedBubbleWebUIController(web_ui) {
  CHECK(base::FeatureList::IsEnabled(features::kOrca));
  content::URLDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                              std::make_unique<MakoSource>());
}
MakoUntrustedUI::~MakoUntrustedUI() = default;

void MakoUntrustedUI::BindInterface(
    mojo::PendingReceiver<input_method::mojom::EditorInstance> receiver) {
  input_method::EditorMediator::Get()->BindEditorInstance(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MakoUntrustedUI)

MakoPageHandler::MakoPageHandler() {
  // TODO(b/289859230): Construct MakoUntrustedUI and show it to the user. Save
  //   a ref to the constructed view to allow for closing it at a later time.
  NOTIMPLEMENTED_LOG_ONCE();
}

MakoPageHandler::~MakoPageHandler() = default;

void MakoPageHandler::CloseUI() {
  // TODO(b/289859230): Use the ref saved from construction to close the webui.
  NOTIMPLEMENTED_LOG_ONCE();
}

void MakoUntrustedUI::Show(Profile* profile) {
  ui::InputMethod* input_method =
      IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  ui::TextInputClient* input_client =
      input_method ? input_method->GetTextInputClient() : nullptr;

  // Does not show mako if there is no input client.
  if (!(input_client)) {
    return;
  }

  gfx::Size window_size = kExtensionWindowSize;
  gfx::Rect caret_bounds =
      input_client ? input_client->GetCaretBounds() : gfx::Rect();

  auto anchor_rect =
      gfx::Rect(caret_bounds.x() + window_size.width(),
                caret_bounds.y() - kPaddingAroundCursor, 0,
                caret_bounds.height() + kPaddingAroundCursor * 2);

  // TODO(b/289969807): 3961 is emoji picker identifier for task manager - we
  // should have a better one for mako
  auto contents_wrapper =
      std::make_unique<BubbleContentsWrapperT<MakoUntrustedUI>>(
          GURL(kChromeUIOrcaURL), profile, 3961);
  contents_wrapper->ReloadWebContents();

  auto bubble_view =
      std::make_unique<MakoDialogView>(std::move(contents_wrapper));
  auto weak_ptr = bubble_view->GetWeakPtr();
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  weak_ptr->SetAnchorRect(anchor_rect);
  weak_ptr->GetBubbleFrameView()->SetPreferredArrowAdjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  weak_ptr->set_adjust_if_offscreen(true);
  weak_ptr->ShowUI();
}
}  // namespace ash
