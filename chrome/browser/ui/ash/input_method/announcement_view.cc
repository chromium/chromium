// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/announcement_view.h"

#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"

namespace ui {
namespace ime {
namespace {

const gfx::Rect kWindowAnchorRect = gfx::Rect(-100000, -100000, 0, 0);

}  // namespace

AnnouncementView::AnnouncementView(gfx::NativeView parent,
                                   const std::u16string& name) {
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCanActivate(false);
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets());
  set_title_margins(gfx::Insets());
  set_shadow(views::BubbleBorder::NO_SHADOW);

  announcement_label_ = AddChildView(std::make_unique<AnnouncementLabel>(name));
  announcement_label_->SetLineHeight(0);

  views::Widget* const widget =
      BubbleDialogDelegate::CreateBubble(base::WrapUnique(this));
  // Set the window size to 0 and put it outside screen to make sure users don't
  // see it.
  widget->SetSize(gfx::Size(0, 0));
  SetAnchorRect(kWindowAnchorRect);
  widget->Show();
}

AnnouncementView::AnnouncementView() = default;
AnnouncementView::~AnnouncementView() = default;

void AnnouncementView::Announce(const std::u16string& message) {
  AnnounceAfterDelay(message, base::Milliseconds(0));
}

void AnnouncementView::AnnounceAfterDelay(const std::u16string& message,
                                          base::TimeDelta delay) {
  DCHECK(announcement_label_);
  if (message.empty()) {
    return;
  }
  announcement_label_->AnnounceAfterDelay(message, delay);
}

BEGIN_METADATA(AnnouncementView)
END_METADATA

}  // namespace ime
}  // namespace ui
