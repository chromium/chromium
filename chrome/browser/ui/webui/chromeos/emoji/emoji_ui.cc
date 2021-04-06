// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_ui.h"

#include "ash/public/cpp/tablet_mode.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper_service.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper_service_factory.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/emoji_picker_resources.h"
#include "chrome/grit/emoji_picker_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/resources/grit/webui_generated_resources.h"

#include <iostream>

namespace {
constexpr gfx::Size kDefaultWindowSize(340, 390);

class EmojiiBubbleDialogView : public WebUIBubbleDialogView {
 public:
  EmojiiBubbleDialogView(
      std::unique_ptr<BubbleContentsWrapper> contents_wrapper)
      : WebUIBubbleDialogView(nullptr, contents_wrapper.get()),
        contents_wrapper_(std::move(contents_wrapper)) {}

 private:
  std::unique_ptr<BubbleContentsWrapper> contents_wrapper_;
};

}  // namespace

namespace chromeos {

EmojiUI::EmojiUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui,
                                    true /* Needed for webui browser tests */) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIEmojiPickerHost);
  source->UseStringsJs();

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, base::make_span(kEmojiPickerResources, kEmojiPickerResourcesSize),
      IDR_EMOJI_PICKER_INDEX_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

EmojiUI::~EmojiUI() = default;

void EmojiUI::Show(Profile* profile) {
  if (ash::TabletMode::Get()->InTabletMode()) {
    ui::ShowTabletModeEmojiPanel();
    return;
  }
  ui::InputMethod* input_method =
      ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  ui::TextInputClient* input_client =
      input_method ? input_method->GetTextInputClient() : nullptr;
  const bool incognito_mode =
      input_client ? !input_client->ShouldDoLearning() : false;
  const gfx::Rect caret_bounds =
      input_client ? input_client->GetCaretBounds() : gfx::Rect();

  // This rect is used for positioning the emoji picker.  All that really
  // matters is a position, so it has 0 height/width
  auto anchor_rect = gfx::Rect(caret_bounds.x() + kDefaultWindowSize.width(),
                               caret_bounds.bottom(), 0, 0);

  // TODO(b/181703133): Refactor so that the webui_bubble_manager can be used
  // here to reduce code duplication.

  auto contents_wrapper = std::make_unique<BubbleContentsWrapperT<EmojiUI>>(
      GURL(chrome::kChromeUIEmojiPickerURL), profile, IDS_ACCNAME_EMOJI_PICKER,
      false /*enable_extension_apis*/);
  // Need to reload the web contents here because the view isn't visible unless
  // ShowUI is called from the JS side.  By reloading, we trigger the JS to
  // eventually call ShowUI().
  contents_wrapper->ReloadWebContents();
  contents_wrapper->GetWebUIController()->incognito_mode_ = incognito_mode;

  auto bubble_view =
      std::make_unique<EmojiiBubbleDialogView>(std::move(contents_wrapper));
  auto weak_ptr = bubble_view->GetWeakPtr();
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  weak_ptr->SetAnchorRect(anchor_rect);
  weak_ptr->set_adjust_if_offscreen(true);
}

WEB_UI_CONTROLLER_TYPE_IMPL(EmojiUI)

void EmojiUI::BindInterface(
    mojo::PendingReceiver<emoji_picker::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void EmojiUI::CreatePageHandler(
    mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<EmojiPageHandler>(
      std::move(receiver), web_ui(), this, incognito_mode_);
}

}  // namespace chromeos
