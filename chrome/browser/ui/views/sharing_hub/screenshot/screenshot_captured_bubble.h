// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SCREENSHOT_SCREENSHOT_CAPTURED_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SCREENSHOT_SCREENSHOT_CAPTURED_BUBBLE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/metadata/view_factory.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}  // namespace content

namespace views {
class ImageView;
class LabelButton;
class MdTextButton;
class View;
}  // namespace views

class Profile;
struct NavigateParams;

namespace sharing_hub {

// Dialog that displays a captured screenshot, and provides the option
// to edit, share, or download.
class ScreenshotCapturedBubble : public LocationBarBubbleDelegateView {
 public:
  METADATA_HEADER(ScreenshotCapturedBubble);
  ScreenshotCapturedBubble(
      views::View* anchor_view,
      content::WebContents* web_contents,
      const gfx::Image& image,
      Profile* profile,
      base::OnceCallback<void(NavigateParams*)> edit_callback);
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

  void EditButtonPressed();

  gfx::Size GetImageSize();

  // Requests navigation to the image editor page.
  // 'screenshot_file_path' is the path to a valid screenshot
  // for use as background, or empty to start with a blank canvas.
  void NavigateToImageEditor(const base::FilePath& screenshot_file_path);

  // Makes a copy of the image to use in button callbacks without worry of
  // dereferencing
  const gfx::Image image_;

  base::WeakPtr<content::WebContents> web_contents_;

  raw_ptr<Profile> profile_;

  base::OnceCallback<void(NavigateParams*)> edit_callback_;

  // Pointers to view widgets; weak.
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION views::ImageView* image_view_ = nullptr;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION views::MdTextButton* download_button_ = nullptr;
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION views::LabelButton* edit_button_ = nullptr;

  base::WeakPtrFactory<ScreenshotCapturedBubble> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(, ScreenshotCapturedBubble, LocationBarBubbleDelegateView)
END_VIEW_BUILDER

}  // namespace sharing_hub

DEFINE_VIEW_BUILDER(, sharing_hub::ScreenshotCapturedBubble)

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SCREENSHOT_SCREENSHOT_CAPTURED_BUBBLE_H_
