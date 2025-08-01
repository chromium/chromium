// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_CAPTURE_CONTENTS_BORDER_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_CAPTURE_CONTENTS_BORDER_HELPER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace gfx {
class Rect;
}

// Helps track whether the contents-border should be drawn.
// TODO(crbug.com/40207590): Support dynamic borders for tabs that only
// have a single capturer.
class TabCaptureContentsBorderHelper {
 public:
  DECLARE_USER_DATA(TabCaptureContentsBorderHelper);

  static TabCaptureContentsBorderHelper* From(
      tabs::TabInterface* tab_interface);

  // Used to identify |TabSharingUIViews| instances to
  // |TabCaptureContentsBorderHelper|, without passing pointers,
  // which is less robust lifetime-wise.
  using CaptureSessionId = uint32_t;

  explicit TabCaptureContentsBorderHelper(tabs::TabInterface& tab_interface);
  ~TabCaptureContentsBorderHelper();

  void OnCapturerAdded(CaptureSessionId capture_session_id);
  void OnCapturerRemoved(CaptureSessionId capture_session_id);

  void VisibilityUpdated();

  void OnRegionCaptureRectChanged(
      CaptureSessionId capture_session_id,
      const std::optional<gfx::Rect>& region_capture_rect);

 private:
  void InitContentsBorderWidget();

  // Decide whether the blue border should be shown, and where.
  void Update();

  // Given that the blue border should be shown, draw it at the right location.
  void UpdateBlueBorderLocation();

  // Determines the correct location of the blue border.
  // 1. If multiple captures of the WebContents exist, the blue border is drawn
  //    around the entire tab's content area.
  // 2. If a single capture of the WebContents exists, the blue border
  //    is dynamically drawn around the captured area of that one capture.
  //    That is, around the entire tab's contents if no cropping is used,
  //    and aroun  the cropped area if cropping is used.
  std::optional<gfx::Rect> GetBlueBorderLocation() const;

  void TabWillDetach(tabs::TabInterface* tab_interface,
                     tabs::TabInterface::DetachReason reason);

  // Each capture session has a unique |uint32_t| ID, and is mapped to
  // an optional<Rect>, whose value is as follows:
  // * If the capture session's last known state was uncropped - nullopt.
  // * Otherwise, the crop-target's position in the last observed frame.
  //   Note that this could be an empty Rect, which is the case when the
  //   capture-target consisted of zero pixels within the viewport.
  std::map<CaptureSessionId, std::optional<gfx::Rect>> session_to_bounds_;

  raw_ref<tabs::TabInterface> tab_interface_;

  base::CallbackListSubscription tab_will_detach_subscription_;

  ui::ScopedUnownedUserData<TabCaptureContentsBorderHelper>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_CAPTURE_CONTENTS_BORDER_HELPER_H_
