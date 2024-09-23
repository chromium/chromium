// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/announcement_label.h"

#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ui {
namespace ime {

AnnouncementLabel::AnnouncementLabel(const std::u16string& name) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kStatus);
  GetViewAccessibility().SetName(name);
  GetViewAccessibility().SetContainerLiveStatus("polite");
  UpdateAccessibleDescription();
}

AnnouncementLabel::~AnnouncementLabel() = default;

void AnnouncementLabel::AnnounceAfterDelay(const std::u16string& text,
                                           base::TimeDelta delay) {
  if (text.empty()) {
    return;
  }
  delay_timer_ = std::make_unique<base::OneShotTimer>();
  delay_timer_->Start(FROM_HERE, delay,
                      base::BindOnce(&AnnouncementLabel::DoAnnouncement,
                                     base::Unretained(this), text));
}

void AnnouncementLabel::DoAnnouncement(const std::u16string text) {
  announcement_text_ = text;

  UpdateAccessibleDescription();

  NotifyAccessibilityEvent(ax::mojom::Event::kLiveRegionChanged,
                           /*send_native_event=*/false);
}

void AnnouncementLabel::UpdateAccessibleDescription() {
  if (announcement_text_.empty()) {
    GetViewAccessibility().RemoveDescription();
  } else {
    GetViewAccessibility().SetDescription(announcement_text_);
  }
}

BEGIN_METADATA(AnnouncementLabel)
END_METADATA

}  // namespace ime
}  // namespace ui
