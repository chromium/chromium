// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_CAPTURE_CONTENTS_BORDER_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_CAPTURE_CONTENTS_BORDER_HELPER_H_

#include <map>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

// Helps track whether the contents-border should be drawn.
// TODO(crbug.com/40207590): Support dynamic borders for tabs that only
// have a single capturer.
class TabCaptureContentsBorderHelper {
 public:
  DECLARE_USER_DATA(TabCaptureContentsBorderHelper);

  // Used to identify |TabSharingUIViews| instances to
  // |TabCaptureContentsBorderHelper|, without passing pointers,
  // which is less robust lifetime-wise.
  using CaptureSessionId = uint32_t;

  explicit TabCaptureContentsBorderHelper(tabs::TabInterface& tab);
  TabCaptureContentsBorderHelper(const TabCaptureContentsBorderHelper&) =
      delete;
  TabCaptureContentsBorderHelper& operator=(
      const TabCaptureContentsBorderHelper&) = delete;
  ~TabCaptureContentsBorderHelper();

  static TabCaptureContentsBorderHelper* From(tabs::TabInterface* tab);
  static TabCaptureContentsBorderHelper* FromWebContents(
      content::WebContents* web_contents);

  void OnCapturerAdded(CaptureSessionId capture_session_id);
  void OnCapturerRemoved(CaptureSessionId capture_session_id);

  void VisibilityUpdated();

  void OnRegionCaptureRectChanged(
      CaptureSessionId capture_session_id,
      const std::optional<gfx::Rect>& region_capture_rect);

  bool IsTabCapturing() const;
  bool ShouldShowBlueBorder() const;

  // Determines the correct location of the ble border.
  // 1. If multiple captures of the WebContents exist, the blue border is drawn
  //    around the entire tab's content area.
  // 2. If a single capture of the WebContents exists, the blue border
  //    is dynamically drawn around the captured area of that one capture.
  //    That is, around the entire tab's contents if no cropping is used,
  //    and aroun  the cropped area if cropping is used.
  std::optional<gfx::Rect> GetBlueBorderLocation() const;

  using CaptureChangeCallbackList = base::RepeatingCallbackList<void(bool)>;
  using CaptureChangeLocationCallbackList =
      base::RepeatingCallbackList<void(std::optional<gfx::Rect>)>;
  base::CallbackListSubscription AddOnTabCaptureChangeCallback(
      CaptureChangeCallbackList::CallbackType callback);
  base::CallbackListSubscription AddOnTabCaptureLocationChangeCallback(
      CaptureChangeLocationCallbackList::CallbackType callback);

 private:
  // Decide whether the blue border should be shown, and where.
  void Update();

  const base::raw_ref<tabs::TabInterface> tab_;

  // Each capture session has a unique |uint32_t| ID, and is mapped to
  // an optional<Rect>, whose value is as follows:
  // * If the capture session's last known state was uncropped - nullopt.
  // * Otherwise, the crop-target's position in the last observed frame.
  //   Note that this could be an empty Rect, which is the case when the
  //   capture-target consisted of zero pixels within the viewport.
  std::map<CaptureSessionId, std::optional<gfx::Rect>> session_to_bounds_;

  CaptureChangeCallbackList capture_change_callbacks_;

  CaptureChangeLocationCallbackList capture_location_change_callbacks_;

  ui::ScopedUnownedUserData<TabCaptureContentsBorderHelper>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_CAPTURE_CONTENTS_BORDER_HELPER_H_
