// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_AI_OVERLAY_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_AI_OVERLAY_TOOLBAR_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;

namespace views {
class ImageButton;
}

class AiOverlayToolbarButton : public PinnedActionToolbarButton {
  METADATA_HEADER(AiOverlayToolbarButton, PinnedActionToolbarButton)

 public:
  AiOverlayToolbarButton(
      Browser* browser,
      actions::ActionId action_id,
      base::WeakPtr<PinnedToolbarActionsContainer> container);
  AiOverlayToolbarButton(const AiOverlayToolbarButton&) = delete;
  AiOverlayToolbarButton& operator=(const AiOverlayToolbarButton&) = delete;
  ~AiOverlayToolbarButton() override;

  // PinnedActionToolbarButton:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void UpdateIcon() override;
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;

  void SetOverlayActive(bool active);

 private:
  void OnOptionsButtonPressed();

  raw_ptr<views::ImageButton> options_button_;
  bool overlay_active_ = false;
};

class AiOverlayToolbarButtonActionViewInterface
    : public PinnedActionToolbarButtonActionViewInterface {
 public:
  explicit AiOverlayToolbarButtonActionViewInterface(
      AiOverlayToolbarButton* action_view);
  ~AiOverlayToolbarButtonActionViewInterface() override = default;

  // PinnedActionToolbarButtonActionViewInterface:
  void ActionItemChangedImpl(actions::ActionItem* action_item) override;

 private:
  raw_ptr<AiOverlayToolbarButton> action_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_AI_OVERLAY_TOOLBAR_BUTTON_H_
