// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SEGMENTED_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SEGMENTED_CONTROL_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

// A segmented control for the vertical tab strip with two mutually exclusive
// segments: Tab Strip (left) and Organizer Panel (right).
class VerticalTabStripSegmentedControl : public views::View {
  METADATA_HEADER(VerticalTabStripSegmentedControl, views::View)

 public:
  enum class Segment {
    kTabStrip,
    kOrganizer,
  };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSegmentedControlElementId);

  explicit VerticalTabStripSegmentedControl(BrowserWindowInterface* browser);
  VerticalTabStripSegmentedControl(const VerticalTabStripSegmentedControl&) =
      delete;
  VerticalTabStripSegmentedControl& operator=(
      const VerticalTabStripSegmentedControl&) = delete;
  ~VerticalTabStripSegmentedControl() override;

  // views::View:
  void OnThemeChanged() override;

  Segment active_segment() const { return active_segment_; }
  views::ImageButton* GetButton(Segment segment);

 private:
  void OnButtonPressed(Segment segment);
  void UpdateActiveSegmentFromController();
  void UpdateButtonIcons();
  void UpdateSegmentBackgrounds();

  const raw_ptr<BrowserWindowInterface> browser_;
  Segment active_segment_ = Segment::kTabStrip;

  raw_ptr<views::ImageButton> tab_strip_button_ = nullptr;
  raw_ptr<views::ImageButton> organizer_button_ = nullptr;

  base::CallbackListSubscription organizer_state_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_SEGMENTED_CONTROL_H_
