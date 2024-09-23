// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULLSCREEN_CONTROL_SUBTLE_NOTIFICATION_VIEW_H_
#define COMPONENTS_FULLSCREEN_CONTROL_SUBTLE_NOTIFICATION_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/image_view.h"
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
  METADATA_HEADER(SubtleNotificationView, views::View)

 public:
  SubtleNotificationView();
  SubtleNotificationView(const SubtleNotificationView&) = delete;
  SubtleNotificationView& operator=(const SubtleNotificationView&) = delete;
  ~SubtleNotificationView() override;

  // Display the |instruction_text| to the user. If |instruction_text| is
  // empty hide the view.
  void UpdateContent(const std::u16string& instruction_text);

  // Display the |instruction_text| to the user, with the |key_images| inside
  // the rectangles that represent keys. |key_images| must either be empty, or
  // the same length as the number of text segments inside pipe characters.
  //
  // If |instruction_text| is empty hide the view.
  void UpdateContent(const std::u16string& instruction_text,
                     std::vector<std::unique_ptr<views::View>> key_images);

  // Creates a Widget containing a SubtleNotificationView.
  static views::Widget* CreatePopupWidget(
      gfx::NativeView parent_view,
      std::unique_ptr<SubtleNotificationView> view);

  std::u16string GetInstructionTextForTest() const;

 private:
  class InstructionView;

  void OnInstructionViewTextChanged();

  void UpdateAccessibleName();

  // Text displayed in the bubble, with optional keyboard keys.
  raw_ptr<InstructionView> instruction_view_;

  base::CallbackListSubscription text_changed_callback_;
};

#endif  // COMPONENTS_FULLSCREEN_CONTROL_SUBTLE_NOTIFICATION_VIEW_H_
