// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_ANNOUNCEMENT_LABEL_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_ANNOUNCEMENT_LABEL_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"

namespace ui {
namespace ime {

// Label used for ChromeVox announcements via live regions.
class AnnouncementLabel : public views::Label {
  METADATA_HEADER(AnnouncementLabel, views::Label)

 public:
  explicit AnnouncementLabel(const std::u16string& name);
  ~AnnouncementLabel() override;

  // Make announcement to ChromeVox with the given text after a delay of N
  // milliseconds specified by the param delay.
  void AnnounceAfterDelay(const std::u16string& text, base::TimeDelta delay);

 private:
  // Callback used for delaying announcements
  void DoAnnouncement(const std::u16string text);

  void UpdateAccessibleDescription();

  // Used to delay the ChromeVox announcements. A delay is required as
  // announcements can "override" each other if they are triggered at
  // a similar time. Providing a delay prevents our announcement being
  // blocked by ChromeVox announcements triggered by text updates (i.e.
  // pressing a key will trigger an announcement of the letter found
  // on that key).
  std::unique_ptr<base::OneShotTimer> delay_timer_;

  std::u16string announcement_text_;
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_ANNOUNCEMENT_LABEL_H_
