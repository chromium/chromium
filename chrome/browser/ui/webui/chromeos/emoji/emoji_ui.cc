// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_ui.h"

#include "ash/public/cpp/tablet_mode.h"
#include "base/supports_user_data.h"
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
constexpr int kPaddingAroundCursor = 8;
constexpr char kEmojiPickerDataKey[] = "EmojiPicker";

class EmojiiBubbleDialogView : public WebUIBubbleDialogView {
 public:
  EmojiiBubbleDialogView(
      std::unique_ptr<BubbleContentsWrapper> contents_wrapper)
      : WebUIBubbleDialogView(nullptr, contents_wrapper.get()),
        contents_wrapper_(std::move(contents_wrapper)) {}

 private:
  std::unique_ptr<BubbleContentsWrapper> contents_wrapper_;
};

struct EmojiManager : public base::SupportsUserData::Data {
  explicit EmojiManager(Profile* profile)
      : manager(nullptr,
                profile,
                GURL(chrome::kChromeUIEmojiPickerURL),
                IDS_ACCNAME_EMOJI_PICKER) {}
  WebUIBubbleManagerT<chromeos::EmojiUI> manager;
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
  if (!profile->GetUserData(kEmojiPickerDataKey)) {
    profile->SetUserData(kEmojiPickerDataKey,
                         std::make_unique<EmojiManager>(profile));
  }
  ui::InputMethod* input_method =
      ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  ui::TextInputClient* input_client =
      input_method ? input_method->GetTextInputClient() : nullptr;
  gfx::Rect caret_bounds =
      input_client ? input_client->GetCaretBounds() : gfx::Rect();

  // In general, try to show emoji picker near the text field. Some text clients
  // (like docs) set the actual input field way off screen in y. Allow for
  // slightly negative y, these will tend to be handled by adjust_if_offscreen,
  // but that can't handle things way off the screen so clamp large negative
  // values to zero to ensure picker is on screen.
  // TODO(b/189041846): Change this to take into account screen size in a more
  // general way.
  if (caret_bounds.y() < -5000) {
    caret_bounds.set_y(0);
  }

  // This rect is used for positioning the emoji picker. It anchors either top
  // right / bottom left of the emoji picker window depending on where the text
  // field is. 8px padding around cursor is applied so that the emoji picker
  // does not cramp existing text.
  auto anchor_rect =
      gfx::Rect(caret_bounds.x() + kDefaultWindowSize.width(),
                caret_bounds.y() - kPaddingAroundCursor, 0,
                caret_bounds.height() + kPaddingAroundCursor * 2);
  EmojiManager* emojiManager =
      static_cast<EmojiManager*>(profile->GetUserData(kEmojiPickerDataKey));
  emojiManager->manager.SetAnchorRect(anchor_rect);
  emojiManager->manager.ShowBubble();
}

WEB_UI_CONTROLLER_TYPE_IMPL(EmojiUI)

void EmojiUI::BindInterface(
    mojo::PendingReceiver<emoji_picker::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void EmojiUI::CreatePageHandler(
    mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<EmojiPageHandler>(std::move(receiver), web_ui(), this);
}

}  // namespace chromeos
