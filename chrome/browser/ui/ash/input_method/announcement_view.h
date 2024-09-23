// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_ANNOUNCEMENT_VIEW_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_ANNOUNCEMENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/input_method/announcement_label.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"

namespace ui {
namespace ime {

// This is en empty view box holding an accessibility label, through which we
// can make ChromeVox announcement incurred by assistive features.
class UI_CHROMEOS_EXPORT AnnouncementView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(AnnouncementView, views::BubbleDialogDelegateView)

 public:
  explicit AnnouncementView(gfx::NativeView parent, const std::u16string& name);
  AnnouncementView(const AnnouncementView&) = delete;
  AnnouncementView& operator=(const AnnouncementView&) = delete;
  ~AnnouncementView() override;

  // TODO(b/324129643): Refactor this to avoid the overridden methods in tests.
  virtual void Announce(const std::u16string& message);
  virtual void AnnounceAfterDelay(const std::u16string& message,
                                  base::TimeDelta delay);

 protected:
  AnnouncementView();

 private:
  FRIEND_TEST_ALL_PREFIXES(AnnouncementViewTest, HeaderAccessibilityProperties);
  raw_ptr<AnnouncementLabel> announcement_label_ = nullptr;
};

BEGIN_VIEW_BUILDER(UI_CHROMEOS_EXPORT,
                   AnnouncementView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ime
}  // namespace ui

DEFINE_VIEW_BUILDER(UI_CHROMEOS_EXPORT, ui::ime::AnnouncementView)

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_ANNOUNCEMENT_VIEW_H_
