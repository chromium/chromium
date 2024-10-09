// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/emoji/emoji_ui.h"

#include <iostream>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/ash/emoji/bubble_utils.h"
#include "chrome/browser/ui/webui/ash/emoji/seal_utils.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/emoji_picker_resources.h"
#include "chrome/grit/emoji_picker_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/seal_resources.h"
#include "chrome/grit/seal_resources_map.h"
#include "chromeos/ash/components/emoji/grit/emoji_map.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/views/view_class_properties.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace {
constexpr gfx::Size kExtensionWindowSize(420, 480);
constexpr int kPaddingAroundCursor = 8;

class EmojiBubbleDialogView : public WebUIBubbleDialogView {
  METADATA_HEADER(EmojiBubbleDialogView, WebUIBubbleDialogView)

 public:
  explicit EmojiBubbleDialogView(
      std::unique_ptr<WebUIContentsWrapper> contents_wrapper,
      gfx::Rect caret_bounds)
      : WebUIBubbleDialogView(nullptr,
                              contents_wrapper->GetWeakPtr(),
                              std::nullopt,
                              views::BubbleBorder::TOP_RIGHT,
                              /*autosize=*/false),
        contents_wrapper_(std::move(contents_wrapper)),
        caret_bounds_(caret_bounds) {
    set_has_parent(false);
    set_corner_radius(20);
    SetProperty(views::kElementIdentifierKey, ash::kEmojiPickerElementId);
  }

  // WebUIBubbleDialogView:
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override {
    WebUIBubbleDialogView::ResizeDueToAutoResize(source, new_size);
    GetWidget()->SetBounds(ash::GetBubbleBoundsAroundCaret(
        caret_bounds_,
        -GetBubbleFrameView()->bubble_border()->GetInsets().ToOutsets(),
        new_size));
  }

 private:
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
  gfx::Rect caret_bounds_;
};

BEGIN_METADATA(EmojiBubbleDialogView)
END_METADATA

emoji_picker::mojom::Category ConvertCategoryEnum(
    ui::EmojiPickerCategory category) {
  switch (category) {
    default:
    case ui::EmojiPickerCategory::kEmojis:
      return emoji_picker::mojom::Category::kEmojis;
    case ui::EmojiPickerCategory::kSymbols:
      return emoji_picker::mojom::Category::kSymbols;
    case ui::EmojiPickerCategory::kEmoticons:
      return emoji_picker::mojom::Category::kEmoticons;
    case ui::EmojiPickerCategory::kGifs:
      return emoji_picker::mojom::Category::kGifs;
  }
}

}  // namespace

namespace ash {

EmojiUIConfig::EmojiUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIEmojiPickerHost) {}

bool EmojiUIConfig::ShouldAutoResizeHost() {
  return true;
}

EmojiUI::EmojiUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui,
                               true /* Needed for webui browser tests */),
      initial_category_(emoji_picker::mojom::Category::kEmojis) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIEmojiPickerHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, base::make_span(kEmojiPickerResources, kEmojiPickerResourcesSize),
      IDR_EMOJI_PICKER_INDEX_HTML);
  source->AddResourcePaths(base::make_span(kEmoji, kEmojiSize));

  // Add seal extra resources.
  if (SealUtils::ShouldEnable()) {
    source->AddResourcePaths(
        base::make_span(kSealResources, kSealResourcesSize));
  }

  // Some web components defined in seal extra resources are based on lit; so
  // we override content security policy here to make them work.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types goog#html parse-html-subset sanitize-inner-html "
      "static-types lit-html lottie-worker-script-loader webui-test-script "
      "webui-test-html print-preview-plugin-loader polymer-html-literal "
      "polymer-template-event-attribute-policy;");

  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
}

EmojiUI::~EmojiUI() = default;

bool EmojiUI::ShouldShow(const ui::TextInputClient* input_client,
                         ui::EmojiPickerFocusBehavior focus_behavior) {
  switch (focus_behavior) {
    case ui::EmojiPickerFocusBehavior::kOnlyShowWhenFocused:
      return input_client != nullptr;
    case ui::EmojiPickerFocusBehavior::kAlwaysShow:
      return true;
  }
}

void EmojiUI::Show(ui::EmojiPickerCategory category,
                   ui::EmojiPickerFocusBehavior focus_behavior,
                   const std::string& initial_query) {
  if (display::Screen::GetScreen()->InTabletMode()) {
    ui::ShowTabletModeEmojiPanel();
    return;
  }

  ui::InputMethod* input_method =
      IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  ui::TextInputClient* input_client =
      input_method ? input_method->GetTextInputClient() : nullptr;

  if (!ShouldShow(input_client, focus_behavior)) {
    return;
  }

  auto* profile = ProfileManager::GetActiveUserProfile();

  if (!profile) {
    return;
  }

  const bool incognito_mode =
      input_client ? !input_client->ShouldDoLearning() : false;
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

  gfx::Size window_size = kExtensionWindowSize;
  // This rect is used for positioning the emoji picker. It anchors either top
  // right / bottom left of the emoji picker window depending on where the text
  // field is. 8px padding around cursor is applied so that the emoji picker
  // does not cramp existing text.
  auto anchor_rect =
      gfx::Rect(caret_bounds.x() + window_size.width(),
                caret_bounds.y() - kPaddingAroundCursor, 0,
                caret_bounds.height() + kPaddingAroundCursor * 2);

  // TODO(b/181703133): Refactor so that the webui_bubble_manager can be used
  // here to reduce code duplication.

  auto contents_wrapper = std::make_unique<WebUIContentsWrapperT<EmojiUI>>(
      GURL(chrome::kChromeUIEmojiPickerURL), profile, IDS_ACCNAME_EMOJI_PICKER);
  // Need to reload the web contents here because the view isn't visible unless
  // ShowUI is called from the JS side.  By reloading, we trigger the JS to
  // eventually call ShowUI().
  contents_wrapper->GetWebUIController()->incognito_mode_ = incognito_mode;
  contents_wrapper->GetWebUIController()->no_text_field_ =
      input_client == nullptr;
  contents_wrapper->GetWebUIController()->initial_category_ =
      ConvertCategoryEnum(category);
  contents_wrapper->GetWebUIController()->initial_query_ = initial_query;

  auto bubble_view = std::make_unique<EmojiBubbleDialogView>(
      std::move(contents_wrapper), caret_bounds);
  auto weak_ptr = bubble_view->GetWeakPtr();
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
}

WEB_UI_CONTROLLER_TYPE_IMPL(EmojiUI)

void EmojiUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void EmojiUI::BindInterface(
    mojo::PendingReceiver<emoji_search::mojom::EmojiSearch> receiver) {
  emoji_search_ = std::make_unique<EmojiSearchProxy>(std::move(receiver));
}

void EmojiUI::BindInterface(
    mojo::PendingReceiver<emoji_picker::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void EmojiUI::BindInterface(
    mojo::PendingReceiver<new_window_proxy::mojom::NewWindowProxy> receiver) {
  new_window_proxy_ =
      std::make_unique<ash::NewWindowProxy>(std::move(receiver));
}

void EmojiUI::BindInterface(
    mojo::PendingReceiver<seal::mojom::SealService> receiver) {
  if (SealUtils::ShouldEnable()) {
    Profile* profile = Profile::FromWebUI(web_ui());
    manta::MantaService* manta_service =
        manta::MantaServiceFactory::GetForProfile(profile);
    seal_service_ = std::make_unique<SealService>(
        /*receiver=*/std::move(receiver),
        /*snapper_provider=*/manta_service->CreateSnapperProvider());
  }
}

void EmojiUI::CreatePageHandler(
    mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<EmojiPageHandler>(
      std::move(receiver), web_ui(), this, incognito_mode_, no_text_field_,
      initial_category_, initial_query_);
}

}  // namespace ash
