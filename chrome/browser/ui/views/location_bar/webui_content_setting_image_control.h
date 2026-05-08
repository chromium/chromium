// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_CONTENT_SETTING_IMAGE_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_CONTENT_SETTING_IMAGE_CONTROL_H_

#include <memory>
#include <variant>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "mojo/public/mojom/base/error.mojom-forward.h"

namespace content {
class WebContents;
}

class ContentSettingImageModel;
class ContentSettingImageViewDelegate;
class WebUIToolbarControlDelegate;

// Manages the ContentSettingImageModels for WebUI toolbar and provides
// state updates to be pushed via ToolbarUIService.
class WebUIContentSettingImageControl {
 public:
  // `delegate` must outlive this instance.
  explicit WebUIContentSettingImageControl(
      ContentSettingImageViewDelegate* setting_view_delegate);
  WebUIContentSettingImageControl(const WebUIContentSettingImageControl&) =
      delete;
  WebUIContentSettingImageControl& operator=(
      const WebUIContentSettingImageControl&) = delete;
  ~WebUIContentSettingImageControl();

  // Generates the default set of models.
  //
  // `webui_delegate` must outlive this instance.
  void Init(WebUIToolbarControlDelegate* webui_delegate = nullptr);

  // Allows injecting a custom set of models for testing.
  void InitForTesting(
      std::vector<std::unique_ptr<ContentSettingImageModel>> models,
      WebUIToolbarControlDelegate* webui_delegate = nullptr);

  // Returns the current state of all content setting images for `web_contents`,
  // auto-opens a bubble if requested, and updates model state for fields that
  // notify changes only once (notify accessibility, auto-open bubble, run
  // animation).
  std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>
  ProcessContentSettingState(content::WebContents* web_contents);

  // Creates and shows a bubble for the given `type`.
  void ShowContentSettingsBubble(
      toolbar_ui_api::mojom::ContentSettingImageType type,
      toolbar_ui_api::mojom::ToolbarUIService::ShowContentSettingsBubbleCallback
          callback);

 private:
  // Like ShowContentSettingsBubble(), but returns the result instead of passing
  // it to a callback.
  base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
  ShowContentSettingsBubbleImpl(
      toolbar_ui_api::mojom::ContentSettingImageType type);

  // Safe since the constructor requires that `setting_view_delegate_` outlive
  // this instance.
  const raw_ptr<ContentSettingImageViewDelegate> setting_view_delegate_ =
      nullptr;
  // Safe since Init() requires that `webui_delegate_` outlive this instance.
  raw_ptr<WebUIToolbarControlDelegate> webui_delegate_ = nullptr;
  std::vector<std::unique_ptr<ContentSettingImageModel>> models_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_CONTENT_SETTING_IMAGE_CONTROL_H_
