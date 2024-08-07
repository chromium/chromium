// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}

namespace views {
class ImageView;
class LabelButton;
class TooltipIcon;
class View;
}  // namespace views

namespace qrcode_generator {

// Dialog that displays a QR code used to share a page or image.
class QRCodeGeneratorBubble : public QRCodeGeneratorBubbleView,
                              public LocationBarBubbleDelegateView,
                              public views::TextfieldController {
  METADATA_HEADER(QRCodeGeneratorBubble, LocationBarBubbleDelegateView)

 public:
  QRCodeGeneratorBubble(views::View* anchor_view,
                        base::WeakPtr<content::WebContents> web_contents,
                        base::OnceClosure on_closing,
                        base::OnceClosure on_back_button_pressed,
                        const GURL& url);
  QRCodeGeneratorBubble(const QRCodeGeneratorBubble&) = delete;
  QRCodeGeneratorBubble& operator=(const QRCodeGeneratorBubble&) = delete;
  ~QRCodeGeneratorBubble() override;

  void Show();

  // QRCodeGeneratorBubbleView:
  void Hide() override;
  void OnThemeChanged() override;

  // Returns a suggested download filename for a given URL.
  // e.g.: www.foo.com may suggest qrcode_foo.png.
  static const std::u16string GetQRCodeFilenameForURL(const GURL& url);

  views::ImageView* image_for_testing() { return qr_code_image_; }
  views::Textfield* textfield_for_testing() { return textfield_url_; }
  views::Label* error_label_for_testing() { return bottom_error_label_; }
  views::LabelButton* download_button_for_testing() { return download_button_; }

  void SetQRCodeErrorForTesting(std::optional<qr_code_generator::Error> error);

 private:
  // Updates and formats QR code, text, and controls.
  void UpdateQRContent();

  // Updates the central QR code image with |qr_image|.
  void UpdateQRImage(gfx::ImageSkia qr_image);

  // Updates the central QR code image with a placeholder.
  void DisplayPlaceholderImage();

  // Shows an error message.
  void DisplayError(qr_code_generator::Error error);

  // Hides all error messages and enables or disables download button.
  void HideErrors(bool enable_download_button);

  // Shrinks the view and sets it not visible.
  void ShrinkAndHideDisplay(views::View* view);

  // LocationBarBubbleDelegateView:
  View* GetInitiallyFocusedView() override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void AddedToWidget() override;

  // TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  const SkBitmap GetBitmap();

  void CopyButtonPressed();

  void DownloadButtonPressed();

  void BackButtonPressed();

  // Unit tests can set `qr_code_error_override_` to inject QR code
  // generation errors.
  std::optional<qr_code_generator::Error> qrcode_error_override_;

  // URL for which the QR code is being generated.
  // Used for validation.
  GURL url_;

  raw_ptr<actions::ActionItem> qrcode_action_item_ = nullptr;

  // Pointers to subviews that we need to update the contents or visibility of
  // after creation.
  raw_ptr<views::ImageView> qr_code_image_ = nullptr;
  raw_ptr<views::Textfield> textfield_url_ = nullptr;
  raw_ptr<views::LabelButton> copy_button_ = nullptr;
  raw_ptr<views::LabelButton> download_button_ = nullptr;
  raw_ptr<views::TooltipIcon> tooltip_icon_ = nullptr;
  raw_ptr<views::Label> center_error_label_ = nullptr;
  raw_ptr<views::Label> bottom_error_label_ = nullptr;

  base::OnceClosure on_closing_;
  base::OnceClosure on_back_button_pressed_;
  base::WeakPtr<content::WebContents> web_contents_;
};

}  // namespace qrcode_generator

#endif  // CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_H_
