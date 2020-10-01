// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_qr_sheet_view.h"

#include "base/base64url.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "chrome/common/qr_code_generator/dino_image.h"
#include "chrome/common/qr_code_generator/qr_code_generator.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

using QRCode = QRCodeGenerator;

namespace {

// QRView displays a QR code.
class QRView : public views::View {
 public:
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

  explicit QRView(base::span<const uint8_t> qr_data) {
    base::Optional<QRCode::GeneratedCode> code = qr_.Generate(qr_data);
    DCHECK(code);
    // The QR Encoder supports dynamic sizing but we expect our data to fit in
    // a version five code.
    DCHECK(code->qr_size == QRCode::V5::kSize);
    qr_tiles_ = code->data;
  }

  ~QRView() override = default;

  void RefreshQRCode(base::span<const uint8_t> new_qr_data) {
    state_ = (state_ + 1) % 6;
    base::Optional<QRCode::GeneratedCode> code =
        qr_.Generate(new_qr_data, /*mask=*/state_);
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

  DISALLOW_COPY_AND_ASSIGN(QRView);
};

// kCompressedPublicKeySize is the size of an X9.62 compressed P-256 public key.
constexpr size_t kCompressedPublicKeySize = 33;

std::array<uint8_t, kCompressedPublicKeySize> SeedToCompressedPublicKey(
    base::span<const uint8_t, 32> seed) {
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_KEY> key(
      EC_KEY_derive_from_secret(p256.get(), seed.data(), seed.size()));
  const EC_POINT* public_key = EC_KEY_get0_public_key(key.get());

  std::array<uint8_t, kCompressedPublicKeySize> ret;
  CHECK_EQ(ret.size(), EC_POINT_point2oct(
                           p256.get(), public_key, POINT_CONVERSION_COMPRESSED,
                           ret.data(), ret.size(), /*ctx=*/nullptr));
  return ret;
}

// Base64EncodedSize returns the number of bytes required to base64 encode an
// input of |input_length| bytes, without padding.
constexpr size_t Base64EncodedSize(size_t input_length) {
  return ((input_length * 4) + 2) / 3;
}

// BuildQRData writes a URL suitable for encoding as a QR to |out_buf|
// and returns a span pointing into that buffer. The URL is generated based on
// |qr_generator_key|.
base::span<uint8_t> BuildQRData(
    uint8_t out_buf[QRCode::V5::kInputBytes],
    base::span<const uint8_t, device::cablev2::kQRKeySize> qr_generator_key) {
  static_assert(device::cablev2::kQRSeedSize <= device::cablev2::kQRKeySize,
                "");
  const std::array<uint8_t, kCompressedPublicKeySize> compressed_public_key =
      SeedToCompressedPublicKey(
          base::span<const uint8_t, device::cablev2::kQRSeedSize>(
              qr_generator_key.data(), device::cablev2::kQRSeedSize));

  uint8_t
      qr_data[EXTENT(compressed_public_key) + device::cablev2::kQRSecretSize];
  memcpy(qr_data, compressed_public_key.data(), compressed_public_key.size());
  static_assert(EXTENT(qr_generator_key) == device::cablev2::kQRSeedSize +
                                                device::cablev2::kQRSecretSize,
                "");
  memcpy(qr_data + compressed_public_key.size(),
         &qr_generator_key[device::cablev2::kQRSeedSize],
         device::cablev2::kQRSecretSize);

  std::string base64_qr_data;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(qr_data),
                        sizeof(qr_data)),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &base64_qr_data);
  static constexpr size_t kEncodedDataLength =
      Base64EncodedSize(sizeof(qr_data));
  DCHECK_EQ(kEncodedDataLength, base64_qr_data.size());

  static constexpr char kPrefix[] = "fido://c1/";
  static constexpr size_t kPrefixLength = sizeof(kPrefix) - 1;

  static_assert(QRCode::V5::kInputBytes >= kPrefixLength + kEncodedDataLength,
                "unexpected QR input length");
  memcpy(out_buf, kPrefix, kPrefixLength);
  memcpy(&out_buf[kPrefixLength], base64_qr_data.data(), kEncodedDataLength);
  return base::span<uint8_t>(out_buf, kPrefixLength + kEncodedDataLength);
}

}  // anonymous namespace

class AuthenticatorQRViewCentered : public views::View {
 public:
  explicit AuthenticatorQRViewCentered(base::span<const uint8_t> qr_data) {
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

  void RefreshQRCode(base::span<const uint8_t> new_qr_data) {
    qr_view_->RefreshQRCode(new_qr_data);
  }

  QRView* qr_view_;
};

AuthenticatorQRSheetView::AuthenticatorQRSheetView(
    std::unique_ptr<AuthenticatorQRSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)),
      qr_generator_key_(static_cast<AuthenticatorQRSheetModel*>(model())
                            ->dialog_model()
                            ->qr_generator_key()) {}

AuthenticatorQRSheetView::~AuthenticatorQRSheetView() = default;

std::unique_ptr<views::View>
AuthenticatorQRSheetView::BuildStepSpecificContent() {
  uint8_t qr_data_buf[QRCode::V5::kInputBytes];
  auto qr_view = std::make_unique<AuthenticatorQRViewCentered>(
      BuildQRData(qr_data_buf, qr_generator_key_));
  qr_view_ = qr_view.get();

  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(600), this,
               &AuthenticatorQRSheetView::Update);
  return qr_view;
}

void AuthenticatorQRSheetView::Update() {
  uint8_t qr_data_buf[QRCode::V5::kInputBytes];
  qr_view_->RefreshQRCode(BuildQRData(qr_data_buf, qr_generator_key_));
}
