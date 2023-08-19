// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_QRCODE_GENERATOR_QRCODE_GENERATOR_BUBBLE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
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
 public:
  METADATA_HEADER(QRCodeGeneratorBubble);
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

  // Given an image |image| of a QR code, adds the required "quiet zone" padding
  // around the outside of it. The |size| size is given in QR code tiles, not in
  // pixels or dips. Both |image| and |size| must be square, and the resulting
  // image is also square.
  static gfx::ImageSkia AddQRCodeQuietZone(const gfx::ImageSkia& image,
                                           const gfx::Size& size,
                                           SkColor background_color);

  views::ImageView* image_for_testing() { return qr_code_image_; }
  views::Textfield* textfield_for_testing() { return textfield_url_; }
  views::Label* error_label_for_testing() { return bottom_error_label_; }
  views::LabelButton* download_button_for_testing() { return download_button_; }

  void SetQRCodeServiceForTesting(
      base::RepeatingCallback<void(mojom::GenerateQRCodeRequestPtr request,
                                   QRImageGenerator::ResponseCallback callback)>
          qrcode_service_override);

 private:
  // Updates and formats QR code, text, and controls.
  void UpdateQRContent();

  // Updates the central QR code image with |qr_image|.
  void UpdateQRImage(gfx::ImageSkia qr_image);

  // Updates the central QR code image with a placeholder.
  void DisplayPlaceholderImage();

  // Shows an error message.
  void DisplayError(mojom::QRCodeGeneratorError error);

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

  // Callback for the request to the OOP service to generate a new image.
  void OnCodeGeneratorResponse(const mojom::GenerateQRCodeResponsePtr response);

  // TODO(https://crbug.com/1431991): Remove this field once there is no
  // internal state (e.g. no `mojo::Remote`) that needs to be maintained by the
  // `QRImageGenerator` class.
  std::unique_ptr<QRImageGenerator> qrcode_service_;

  // Unit tests can set `qr_code_service_override_` to intercept QR code
  // requests and inject test-controlled QR code responses.
  //
  // Rationale for using a `RepeatingCallback` instead of implementing
  // dependency injection using virtual methods: 1) `GenerateQRCode` will become
  // a free function after shipping https://crbug.com/1431991, 2) in general
  // `RepeatingCallback` is equivalent to a pure interface with a single method,
  // (note that `RepeatingCallback` below has the same signature as
  // `GenerateQRCode`).
  base::RepeatingCallback<void(mojom::GenerateQRCodeRequestPtr request,
                               QRImageGenerator::ResponseCallback callback)>
      qrcode_service_override_;

  // URL for which the QR code is being generated.
  // Used for validation.
  GURL url_;

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
