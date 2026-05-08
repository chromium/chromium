// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_content_setting_image_control.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_view_delegate.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace {

using ImageType = ContentSettingImageModel::ImageType;
using Code = mojo_base::mojom::Code;
using Error = mojo_base::mojom::Error;

toolbar_ui_api::mojom::ContentSettingImageStatePtr GetImageStateForModel(
    ContentSettingImageModel* model,
    content::WebContents* web_contents) {
  model->Update(web_contents);

  if (!model->is_visible()) {
    return nullptr;
  }

  auto state = toolbar_ui_api::mojom::ContentSettingImageState::New();
  state->type = model->image_type();
  state->is_blocked = model->is_blocked();
  state->tooltip = model->get_tooltip();
  if (model->explanatory_string_id() != 0) {
    state->explanatory_string =
        l10n_util::GetStringUTF16(model->explanatory_string_id());
  }
  if (model->AccessibilityAnnouncementStringId() != 0) {
    state->accessibility_string =
        l10n_util::GetStringUTF16(model->AccessibilityAnnouncementStringId());
  }
  state->should_run_animation = model->ShouldRunAnimation(web_contents);

  return state;
}

}  // namespace

WebUIContentSettingImageControl::WebUIContentSettingImageControl(
    ContentSettingImageViewDelegate* setting_view_delegate)
    : setting_view_delegate_(setting_view_delegate) {}

WebUIContentSettingImageControl::~WebUIContentSettingImageControl() = default;

void WebUIContentSettingImageControl::Init(
    WebUIToolbarControlDelegate* webui_delegate) {
  models_ = ContentSettingImageModel::GenerateContentSettingImageModels();
  webui_delegate_ = webui_delegate;
}

void WebUIContentSettingImageControl::InitForTesting(
    std::vector<std::unique_ptr<ContentSettingImageModel>> models,
    WebUIToolbarControlDelegate* webui_delegate) {
  models_ = std::move(models);
  webui_delegate_ = webui_delegate;
}

std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>
WebUIContentSettingImageControl::ProcessContentSettingState(
    content::WebContents* web_contents) {
  std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr> state;
  if (!web_contents) {
    return state;
  }

  for (auto& model : models_) {
    auto image_state = GetImageStateForModel(model.get(), web_contents);
    if (image_state) {
      state.push_back(std::move(image_state));

      // After gathering the state, we need to notify the model that it's been
      // shown / notified so it doesn't repeat itself in the next update.
      if (model->ShouldNotifyAccessibility(web_contents)) {
        auto name = l10n_util::GetStringUTF16(
            model->AccessibilityAnnouncementStringId());

        if (webui_delegate_) {
          webui_delegate_->AnnounceAlert(l10n_util::GetStringFUTF16(
              IDS_A11Y_INDICATORS_ANNOUNCEMENT, name,
              l10n_util::GetStringUTF16(IDS_A11Y_OMNIBOX_CHIP_HINT)));
        }

        model->AccessibilityWasNotified(web_contents);
      }
      if (model->ShouldAutoOpenBubble(web_contents)) {
        auto result = ShowContentSettingsBubbleImpl(model->image_type());
        CHECK(result.has_value()) << result.error()->message;
        model->SetBubbleWasAutoOpened(web_contents);
      }
      if (model->ShouldRunAnimation(web_contents)) {
        // TODO: crbug.com/489109708 - Investigate why the animation sometimes
        // re-runs when typing in the location bar post-animation.
        int string_id = model->explanatory_string_id();
        if (string_id && webui_delegate_) {
          webui_delegate_->AnnounceAlert(l10n_util::GetStringUTF16(string_id));
        }
        model->SetAnimationHasRun(web_contents);
      }
    }
  }

  return state;
}

void WebUIContentSettingImageControl::ShowContentSettingsBubble(
    ImageType type,
    toolbar_ui_api::mojom::ToolbarUIService::ShowContentSettingsBubbleCallback
        callback) {
  std::move(callback).Run(ShowContentSettingsBubbleImpl(type));
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUIContentSettingImageControl::ShowContentSettingsBubbleImpl(ImageType type) {
  content::WebContents* web_contents =
      setting_view_delegate_->GetContentSettingWebContents();
  if (!web_contents) {
    return std::monostate();
  }

  ContentSettingImageModel* model = nullptr;
  for (auto& m : models_) {
    if (m->image_type() == type) {
      model = m.get();
      break;
    }
  }

  if (!model) {
    return base::unexpected(Error::New(
        Code::kFailedPrecondition,
        base::StringPrintf("WebUIContentSettingImageControl: cannot create "
                           "bubble for non-existent icon for type: %d",
                           static_cast<int32_t>(type))));
  }

  std::unique_ptr<ContentSettingBubbleModel> bubble_model =
      model->CreateBubbleModel(
          setting_view_delegate_->GetContentSettingBubbleModelDelegate(),
          web_contents);

  // Create and show the bubble contents.
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents);
  if (!browser) {
    // The window may be shutting down, so not an error.
    return std::monostate();
  }
  // TODO: crbug.com/500069767 - When we support multiple window types, change
  // this from hard-coded to something that get the right anchor for the
  // current window type.
  views::BubbleAnchor anchor(
      BrowserElements::From(browser)->GetElement(kLocationBarElementId));

  auto bubble_contents = std::make_unique<ContentSettingBubbleContents>(
      std::move(bubble_model), web_contents, anchor,
      views::BubbleBorder::TOP_RIGHT);
  bubble_contents->SetHighlightedElement(model->GetElementIdentifier());

  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_contents))
      ->Show();

  return std::monostate();
}
