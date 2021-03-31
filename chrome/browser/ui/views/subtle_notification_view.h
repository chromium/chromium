// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SUBTLE_NOTIFICATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SUBTLE_NOTIFICATION_VIEW_H_

#include <memory>
#include <string>

#include "ui/gfx/native_widget_types.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Widget;
}

// A transient, transparent notification bubble that appears at the top of the
// browser window to give the user a short instruction (e.g., "Press Esc to exit
// full screen"). Unlike a full notification, a subtle notification
// auto-dismisses after a short period of time. It also has special
// functionality for displaying keyboard shortcuts (rendering the keys inside a
// rounded rectangle).
class SubtleNotificationView : public views::View {
 public:
  METADATA_HEADER(SubtleNotificationView);
  SubtleNotificationView();
  SubtleNotificationView(const SubtleNotificationView&) = delete;
  SubtleNotificationView& operator=(const SubtleNotificationView&) = delete;
  ~SubtleNotificationView() override;

  // Display the |instruction_text| to the user. If |instruction_text| is
  // empty hide the view.
  void UpdateContent(const std::u16string& instruction_text);

  // Creates a Widget containing a SubtleNotificationView.
  static views::Widget* CreatePopupWidget(
      gfx::NativeView parent_view,
      std::unique_ptr<SubtleNotificationView> view);
  // views::View
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  class InstructionView;

  // Text displayed in the bubble, with optional keyboard keys.
  InstructionView* instruction_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SUBTLE_NOTIFICATION_VIEW_H_
