// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class TabCloseButton;
class TabCollectionNode;
class VerticalTabIcon;

namespace views {
class Label;
}

// View for a vertical tabstrip's tab.
class VerticalTabView : public views::View,
                        public views::LayoutDelegate,
                        public AlertIndicatorButton::Delegate {
  METADATA_HEADER(VerticalTabView, views::View)

 public:
  explicit VerticalTabView(TabCollectionNode* collection_node);
  VerticalTabView(const VerticalTabView&) = delete;
  VerticalTabView& operator=(const VerticalTabView&) = delete;
  ~VerticalTabView() override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // AlertIndicatorButton::Delegate
  bool ShouldEnableMuteToggle(int required_width) override;
  void ToggleTabAudioMute() override;
  bool IsApparentlyActive() const override;
  void AlertStateChanged() override;

  VerticalTabIcon* icon_for_testing() { return icon_; }
  AlertIndicatorButton* alert_indicator_for_testing() {
    return alert_indicator_;
  }
  TabCloseButton* close_button_for_testing() { return close_button_; }

 private:
  void ResetCollectionNode();

  void OnDataChanged();

  void UpdateAlertIndicatorVisibility();

  raw_ptr<TabCollectionNode> collection_node_ = nullptr;

  base::CallbackListSubscription node_destroyed_subscription_;
  base::CallbackListSubscription data_changed_subscription_;

  const raw_ptr<VerticalTabIcon> icon_;
  const raw_ptr<views::Label> title_;
  const raw_ptr<AlertIndicatorButton> alert_indicator_;
  const raw_ptr<TabCloseButton> close_button_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_
