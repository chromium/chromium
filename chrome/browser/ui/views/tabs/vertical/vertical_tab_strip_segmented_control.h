// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SEGMENTED_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SEGMENTED_CONTROL_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

class BrowserWindowInterface;
class ExpandOnHoverLock;

namespace views {
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

// A segmented control for the vertical tab strip with two mutually exclusive
// segments: Tab Strip (left) and Organizer Panel (right).
class VerticalTabStripSegmentedControl : public views::View,
                                         public views::ContextMenuController,
                                         public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(VerticalTabStripSegmentedControl, views::View)

 public:
  enum class Segment {
    kTabStrip,
    kOrganizer,
  };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSegmentedControlElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTabSearchUnpinMenuItem);

  explicit VerticalTabStripSegmentedControl(BrowserWindowInterface* browser);
  VerticalTabStripSegmentedControl(const VerticalTabStripSegmentedControl&) =
      delete;
  VerticalTabStripSegmentedControl& operator=(
      const VerticalTabStripSegmentedControl&) = delete;
  ~VerticalTabStripSegmentedControl() override;

  // views::View:
  void OnThemeChanged() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  Segment active_segment() const { return active_segment_; }
  views::ImageButton* GetButton(Segment segment);

  ui::SimpleMenuModel* menu_model_for_testing() { return menu_model_.get(); }

 private:
  void OnButtonPressed(Segment segment);
  void UpdateActiveSegmentFromController();
  void UpdateButtonIcons();
  void UpdateSegmentBackgrounds();
  void OnMenuClosed();

  const raw_ptr<BrowserWindowInterface> browser_;
  Segment active_segment_ = Segment::kTabStrip;

  raw_ptr<views::ImageButton> tab_strip_button_ = nullptr;
  raw_ptr<views::ImageButton> organizer_button_ = nullptr;

  base::CallbackListSubscription organizer_state_subscription_;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  std::unique_ptr<ExpandOnHoverLock> expand_on_hover_lock_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SEGMENTED_CONTROL_H_
