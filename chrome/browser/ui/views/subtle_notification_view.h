// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SUBTLE_NOTIFICATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SUBTLE_NOTIFICATION_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/native_widget_types.h"
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
  SubtleNotificationView();
  ~SubtleNotificationView() override;

  // Display the |instruction_text| to the user. If |instruction_text| is
  // empty hide the view.
  void UpdateContent(const base::string16& instruction_text);

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

  DISALLOW_COPY_AND_ASSIGN(SubtleNotificationView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SUBTLE_NOTIFICATION_VIEW_H_
