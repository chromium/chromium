// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_CONTENT_SETTING_IMAGE_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_CONTENT_SETTING_IMAGE_CONTROL_H_

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_reopen_suppressor.h"
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

  ContentSettingImageModel* GetModel(
      toolbar_ui_api::mojom::ContentSettingImageType type) const;

  // Called when the user presses the mouse button on the chip.
  void OnContentSettingImagePointerDown(
      toolbar_ui_api::mojom::ContentSettingImageType type);

  // Creates and shows a bubble for the given `type`.
  void ShowContentSettingsBubble(
      toolbar_ui_api::mojom::ContentSettingImageType type,
      bool is_pointer_interaction,
      toolbar_ui_api::mojom::ToolbarUIService::ShowContentSettingsBubbleCallback
          callback);

  bool IsBubbleShowing() const;
  bool TestPressed(size_t index);
  bool IsBubbleShowing(size_t index) const;
  bool IsContentSettingImageVisible(size_t index) const;

  void SetSuppressionThresholdForTesting(base::TimeDelta threshold) {
    bubble_reopen_suppressor_.SetSuppressionThresholdForTesting(threshold);
  }

  // Called when the WebUI animation for `type` has finished.
  void OnContentSettingImageAnimationEnded(
      toolbar_ui_api::mojom::ContentSettingImageType type);

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
  friend class WebUIContentSettingImageControlTest;

  // Safe since Init() requires that `webui_delegate_` outlive this instance.
  raw_ptr<WebUIToolbarControlDelegate> webui_delegate_ = nullptr;
  std::vector<std::unique_ptr<ContentSettingImageModel>> models_;
  std::optional<toolbar_ui_api::mojom::ContentSettingImageType>
      last_tracked_bubble_type_;

  WebUIBubbleReopenSuppressor bubble_reopen_suppressor_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_WEBUI_CONTENT_SETTING_IMAGE_CONTROL_H_
