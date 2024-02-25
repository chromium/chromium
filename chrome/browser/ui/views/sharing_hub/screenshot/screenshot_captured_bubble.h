// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SCREENSHOT_SCREENSHOT_CAPTURED_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SCREENSHOT_SCREENSHOT_CAPTURED_BUBBLE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/view_factory.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class ImageView;
class MdTextButton;
class View;
}  // namespace views

class Profile;

namespace sharing_hub {

// Dialog that displays a captured screenshot, and provides the option
// to share or download.
class ScreenshotCapturedBubble : public LocationBarBubbleDelegateView {
  METADATA_HEADER(ScreenshotCapturedBubble, LocationBarBubbleDelegateView)

 public:
  ScreenshotCapturedBubble(views::View* anchor_view,
                           content::WebContents* web_contents,
                           const gfx::Image& image,
                           Profile* profile);
  ScreenshotCapturedBubble(const ScreenshotCapturedBubble&) = delete;
  ScreenshotCapturedBubble& operator=(const ScreenshotCapturedBubble&) = delete;
  ~ScreenshotCapturedBubble() override;

  void OnThemeChanged() override;

  void Show();

 private:
  // LocationBarBubbleDelegateView:
  View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  static const std::u16string GetFilenameForURL(const GURL& url);

  // views::BubbleDialogDelegateView:
  void Init() override;

  void DownloadButtonPressed();

  gfx::Size GetImageSize();

  // Makes a copy of the image to use in button callbacks without worry of
  // dereferencing
  const gfx::Image image_;

  base::WeakPtr<content::WebContents> web_contents_;

  raw_ptr<Profile> profile_;

  // Pointers to view widgets; weak.
  raw_ptr<views::ImageView> image_view_ = nullptr;
  raw_ptr<views::MdTextButton> download_button_ = nullptr;

  base::WeakPtrFactory<ScreenshotCapturedBubble> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(, ScreenshotCapturedBubble, LocationBarBubbleDelegateView)
END_VIEW_BUILDER

}  // namespace sharing_hub

DEFINE_VIEW_BUILDER(, sharing_hub::ScreenshotCapturedBubble)

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SCREENSHOT_SCREENSHOT_CAPTURED_BUBBLE_H_
