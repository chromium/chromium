// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_capture_contents_border_helper.h"

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "content/public/browser/browser_thread.h"

namespace {
constexpr int kMinContentsBorderWidth = 20;
constexpr int kMinContentsBorderHeight = 20;
}  // namespace

TabCaptureContentsBorderHelper::TabCaptureContentsBorderHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<TabCaptureContentsBorderHelper>(
          *web_contents) {}

TabCaptureContentsBorderHelper::~TabCaptureContentsBorderHelper() = default;

void TabCaptureContentsBorderHelper::OnCapturerAdded(
    CaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!base::Contains(session_to_bounds_, capture_session_id));

  session_to_bounds_[capture_session_id] = std::nullopt;

  Update();
}

void TabCaptureContentsBorderHelper::OnCapturerRemoved(
    CaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/40213800): Destroy widget when the size of
  // `session_to_bounds_` hits 0. Same for `this`.
  session_to_bounds_.erase(capture_session_id);

  Update();
}

void TabCaptureContentsBorderHelper::VisibilityUpdated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Update();
}

void TabCaptureContentsBorderHelper::OnRegionCaptureRectChanged(
    CaptureSessionId capture_session_id,
    const std::optional<gfx::Rect>& region_capture_rect) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(base::Contains(session_to_bounds_, capture_session_id));

  if (region_capture_rect &&
      region_capture_rect->width() >= kMinContentsBorderWidth &&
      region_capture_rect->height() >= kMinContentsBorderHeight) {
    session_to_bounds_[capture_session_id] = region_capture_rect;
  } else {
    session_to_bounds_[capture_session_id] = std::nullopt;
  }

  capture_location_change_callbacks_.Notify(
      session_to_bounds_[capture_session_id]);
}

bool TabCaptureContentsBorderHelper::IsTabCapturing() const {
  return !session_to_bounds_.empty();
}

bool TabCaptureContentsBorderHelper::ShouldShowBlueBorder() const {
  bool show_border = IsTabCapturing();
#if BUILDFLAG(IS_CHROMEOS)
  show_border = show_border && base::FeatureList::IsEnabled(
                                   features::kTabCaptureBlueBorderCrOS);
#endif  // BUILDFLAG(IS_CHROMEOS)
  return show_border;
}

base::CallbackListSubscription
TabCaptureContentsBorderHelper::AddOnTabCaptureChangeCallback(
    CaptureChangeCallbackList::CallbackType callback) {
  return capture_change_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription
TabCaptureContentsBorderHelper::AddOnTabCaptureLocationChangeCallback(
    CaptureChangeLocationCallbackList::CallbackType callback) {
  return capture_location_change_callbacks_.Add(std::move(callback));
}

void TabCaptureContentsBorderHelper::Update() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(IS_CHROMEOS)
  // The blue border behavior used to be problematic on ChromeOS - see
  // crbug.com/1320262 and crbug.com/1030925. This check serves as a means of
  // flag-disabling this feature in case of possible future regressions.
  if (!base::FeatureList::IsEnabled(features::kTabCaptureBlueBorderCrOS)) {
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  content::WebContents* const web_contents = &GetWebContents();

  Browser* const browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return;
  }

  const bool tab_visible =
      (web_contents == browser->tab_strip_model()->GetActiveWebContents());
  const bool contents_border_needed = tab_visible && IsTabCapturing();

  if (contents_border_needed) {
    capture_location_change_callbacks_.Notify(GetBlueBorderLocation());
  }

  capture_change_callbacks_.Notify(contents_border_needed);
}

std::optional<gfx::Rect> TabCaptureContentsBorderHelper::GetBlueBorderLocation()
    const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsTabCapturing()) << "No blue border should be shown.";

  // The border should only track the cropped-to contents when there is exactly
  // one capture session. If there are more, fall back on drawing the border
  // around the entire tab.
  return (session_to_bounds_.size() == 1u) ? session_to_bounds_.begin()->second
                                           : std::nullopt;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabCaptureContentsBorderHelper);
