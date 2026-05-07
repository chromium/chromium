// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/webui_content_setting_image_control.h"

#include <optional>
#include <variant>

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
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/content_settings/core/common/features.h"
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
    state->accessibility_announcement_string =
        l10n_util::GetStringUTF16(model->AccessibilityAnnouncementStringId());
  }
  state->should_notify_accessibility =
      model->ShouldNotifyAccessibility(web_contents);
  state->should_run_animation = model->ShouldRunAnimation(web_contents);

  return state;
}

}  // namespace

WebUIContentSettingImageControl::WebUIContentSettingImageControl(
    ContentSettingImageViewDelegate* delegate)
    : delegate_(delegate) {}

WebUIContentSettingImageControl::~WebUIContentSettingImageControl() = default;

void WebUIContentSettingImageControl::Init() {
  models_ = ContentSettingImageModel::GenerateContentSettingImageModels();
}

void WebUIContentSettingImageControl::InitForTesting(
    std::vector<std::unique_ptr<ContentSettingImageModel>> models) {
  models_ = std::move(models);
}

std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>
WebUIContentSettingImageControl::ProcessContentSettingState(
    content::WebContents* web_contents) {
  std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr> state;
  if (!web_contents) {
    return state;
  }

  for (auto& model : models_) {
    // The activity indicators (camera, mic) are drawn on the left side of the
    // location bar, managed by the Permissions Dashboard, so we don't include
    // them in the right hand side content setting images here.
    if (model->image_type() == ImageType::kMediaStream) {
      continue;
    }

    auto image_state = GetImageStateForModel(model.get(), web_contents);
    if (image_state) {
      state.push_back(std::move(image_state));

      // After gathering the state, we need to notify the model that it's been
      // shown / notified so it doesn't repeat itself in the next update.
      if (model->ShouldNotifyAccessibility(web_contents)) {
        model->AccessibilityWasNotified(web_contents);
      }
      if (model->ShouldAutoOpenBubble(web_contents)) {
        auto result = ShowContentSettingsBubbleImpl(model->image_type());
        CHECK(result.has_value()) << result.error()->message;
        model->SetBubbleWasAutoOpened(web_contents);
      }
      if (model->ShouldRunAnimation(web_contents)) {
        model->SetAnimationHasRun(web_contents);
      }
    }
  }

  return state;
}

ContentSettingImageModel* WebUIContentSettingImageControl::GetModel(
    ImageType type) const {
  auto it =
      std::ranges::find(models_, type, &ContentSettingImageModel::image_type);
  return it != models_.end() ? it->get() : nullptr;
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
      delegate_->GetContentSettingWebContents();
  if (!web_contents) {
    return std::monostate();
  }

  ContentSettingImageModel* model = GetModel(type);

  if (!model) {
    return base::unexpected(Error::New(
        Code::kFailedPrecondition,
        base::StringPrintf("WebUIContentSettingImageControl: cannot create "
                           "bubble for non-existent icon for type: %d",
                           static_cast<int32_t>(type))));
  }

  std::unique_ptr<ContentSettingBubbleModel> bubble_model =
      model->CreateBubbleModel(
          delegate_->GetContentSettingBubbleModelDelegate(), web_contents);

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
