// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_qr_sheet_view.h"

#include "base/base64url.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "components/qr_code_generator/dino_image.h"
#include "components/qr_code_generator/qr_code_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

using QRCode = QRCodeGenerator;

namespace {

// kMinimumQRVersion is the minimum QR version (i.e. size) that we support.
// The amount of input already precludes smaller versions with the current
// encoder but it's possible that a low-ECC configuration could be added that
// would otherwise cause a smaller version to be used. This minimum ensures
// that the UI sizing remains constant and that the dino image doesn't
// obscure too much of the QR code
constexpr int kMinimumQRVersion = 5;

// QRView displays a QR code.
class QRView : public views::View {
 public:
  METADATA_HEADER(QRView);

  // kTilePixels is the height and width, in pixels, of a single tile from the
  // QR code.
  static constexpr int kTilePixels = 10;
  // kDinoTilePixels is the height and width, in pixels, of a single bit from
  // the dino image.
  static constexpr int kDinoTilePixels = 3;
  // kMid is the pixel offset from the top (or left) to the middle of the
  // displayed QR code.
  static constexpr int kMid = (kTilePixels * (2 + QRCode::V5::kSize + 2)) / 2;
  // kDinoX is the x-coordinate of the dino image.
  static constexpr int kDinoX =
      kMid - (dino_image::kDinoWidth * kDinoTilePixels) / 2;
  // kDinoY is the y-coordinate of the dino image.
  static constexpr int kDinoY =
      kMid - (dino_image::kDinoHeight * kDinoTilePixels) / 2;

  explicit QRView(const std::string& qr_string) {
    CHECK_LE(qr_string.size(), QRCodeGenerator::V5::kInputBytes);

    base::Optional<QRCode::GeneratedCode> code = qr_.Generate(
        base::as_bytes(base::make_span(qr_string)), kMinimumQRVersion);
    DCHECK(code);
    // The QR Encoder supports dynamic sizing but we expect our data to fit in
    // a version five code.
    DCHECK(code->qr_size == QRCode::V5::kSize);
    qr_tiles_ = code->data;
  }

  QRView(const QRView&) = delete;
  QRView& operator=(const QRView&) = delete;
  ~QRView() override = default;

  void RefreshQRCode(const std::string& qr_string) {
    CHECK_LE(qr_string.size(), QRCodeGenerator::V5::kInputBytes);

    state_ = (state_ + 1) % 6;
    base::Optional<QRCode::GeneratedCode> code =
        qr_.Generate(base::as_bytes(base::make_span(qr_string)),
                     kMinimumQRVersion, /*mask=*/state_);
    DCHECK(code);
    qr_tiles_ = code->data;
    SchedulePaint();
  }

  // View:
  gfx::Size CalculatePreferredSize() const override {
    // A two-tile border is required around the QR code.
    return gfx::Size((2 + QRCode::V5::kSize + 2) * kTilePixels,
                     (2 + QRCode::V5::kSize + 2) * kTilePixels);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const SkColor off = SkColorSetARGB(0xff, 0xff, 0xff, 0xff);

    // kV is the intensity of the colors in the QR code.
    constexpr uint8_t kV = 0x70;
    SkColor on;
    switch (state_) {
      case 0:
        on = SkColorSetARGB(0xff, kV, 0, 0);
        break;
      case 1:
        on = SkColorSetARGB(0xff, 0, kV, 0);
        break;
      case 2:
        on = SkColorSetARGB(0xff, 0, 0, kV);
        break;
      case 3:
        on = SkColorSetARGB(0xff, kV, kV, 0);
        break;
      case 4:
        on = SkColorSetARGB(0xff, kV, 0, kV);
        break;
      case 5:
        on = SkColorSetARGB(0xff, 0, kV, kV);
        break;
    }

    // Draw the two-tile border around the edge.
    // Top.
    canvas->FillRect(gfx::Rect(0, 0, (2 + QRCode::V5::kSize + 2) * kTilePixels,
                               2 * kTilePixels),
                     off);
    // Bottom.
    canvas->FillRect(
        gfx::Rect(0, (2 + QRCode::V5::kSize) * kTilePixels,
                  (2 + QRCode::V5::kSize + 2) * kTilePixels, 2 * kTilePixels),
        off);
    // Left
    canvas->FillRect(gfx::Rect(0, 2 * kTilePixels, 2 * kTilePixels,
                               QRCode::V5::kSize * kTilePixels),
                     off);
    // Right
    canvas->FillRect(
        gfx::Rect((2 + QRCode::V5::kSize) * kTilePixels, 2 * kTilePixels,
                  2 * kTilePixels, QRCode::V5::kSize * kTilePixels),
        off);

    // Paint the QR code.
    int index = 0;
    for (int y = 0; y < QRCode::V5::kSize; y++) {
      for (int x = 0; x < QRCode::V5::kSize; x++) {
        SkColor tile_color = qr_tiles_[index++] & 1 ? on : off;
        canvas->FillRect(gfx::Rect((x + 2) * kTilePixels, (y + 2) * kTilePixels,
                                   kTilePixels, kTilePixels),
                         tile_color);
      }
    }

    PaintDinoSegment(
        canvas,
        (state_ & 1) ? dino_image::kDinoHeadLeft : dino_image::kDinoHeadRight,
        dino_image::kDinoHeadHeight, 0);
    PaintDinoSegment(canvas, dino_image::kDinoBody,
                     dino_image::kDinoHeight - dino_image::kDinoHeadHeight,
                     dino_image::kDinoHeadHeight);
  }

 private:
  void PaintDinoSegment(gfx::Canvas* canvas,
                        const uint8_t* data,
                        const int rows,
                        const int y_offset) {
    const SkColor color = SkColorSetARGB(0xff, 0x00, 0x00, 0x00);

    for (int y = 0; y < rows; y++) {
      uint8_t current_byte;
      int bits = 0;

      for (int x = 0; x < dino_image::kDinoWidth; x++) {
        if (bits == 0) {
          current_byte = *data++;
          bits = 8;
        }
        const bool is_set = (current_byte & 128) != 0;
        current_byte <<= 1;
        bits--;

        if (is_set) {
          canvas->FillRect(gfx::Rect(kDinoX + x * kDinoTilePixels,
                                     kDinoY + (y + y_offset) * kDinoTilePixels,
                                     kDinoTilePixels, kDinoTilePixels),
                           color);
        }
      }
    }
  }

  QRCode qr_;
  base::span<const uint8_t> qr_tiles_;
  int state_ = 0;
};

BEGIN_METADATA(QRView, views::View)
END_METADATA

}  // anonymous namespace

class AuthenticatorQRViewCentered : public views::View {
 public:
  METADATA_HEADER(AuthenticatorQRViewCentered);
  explicit AuthenticatorQRViewCentered(const std::string& qr_data) {
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    qr_view_ = new QRView(qr_data);
    AddChildView(qr_view_);
  }

  void RefreshQRCode(const std::string& new_qr_data) {
    qr_view_->RefreshQRCode(new_qr_data);
  }

  QRView* qr_view_;
};

BEGIN_METADATA(AuthenticatorQRViewCentered, views::View)
END_METADATA

AuthenticatorQRSheetView::AuthenticatorQRSheetView(
    std::unique_ptr<AuthenticatorQRSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)),
      qr_string_(static_cast<AuthenticatorQRSheetModel*>(model())
                     ->dialog_model()
                     ->cable_qr_string()) {}

AuthenticatorQRSheetView::~AuthenticatorQRSheetView() = default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorQRSheetView::BuildStepSpecificContent() {
  auto qr_view = std::make_unique<AuthenticatorQRViewCentered>(qr_string_);
  qr_view_ = qr_view.get();

  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(600), this,
               &AuthenticatorQRSheetView::Update);
  return std::make_pair(std::move(qr_view), AutoFocus::kYes);
}

void AuthenticatorQRSheetView::Update() {
  qr_view_->RefreshQRCode(qr_string_);
}
