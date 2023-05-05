// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_QRCODE_GENERATOR_QRCODE_GENERATOR_SERVICE_IMPL_H_
#define CHROME_SERVICES_QRCODE_GENERATOR_QRCODE_GENERATOR_SERVICE_IMPL_H_

#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace qrcode_generator {

// qrcode_generator.QRCodeGenerator handler.
//
// This handler accepts a potentially untrusted URL string and generates
// a QR code for it.
// It is intended to operate in an out-of-browser-process service.
class QRCodeGeneratorServiceImpl : public mojom::QRCodeGeneratorService {
 public:
  explicit QRCodeGeneratorServiceImpl(
      mojo::PendingReceiver<mojom::QRCodeGeneratorService> receiver);

  QRCodeGeneratorServiceImpl(const QRCodeGeneratorServiceImpl&) = delete;
  QRCodeGeneratorServiceImpl& operator=(const QRCodeGeneratorServiceImpl&) =
      delete;

  ~QRCodeGeneratorServiceImpl() override;

 private:
  // chrome::mojom::QRCodeGeneratorService override.
  void GenerateQRCode(mojom::GenerateQRCodeRequestPtr request,
                      GenerateQRCodeCallback callback) override;

  // Renders dino data into a 1x bitmap, |dino_bitmap_|, owned by the class.
  // This is simpler and faster than repainting it from static source data
  // each time.
  void InitializeDinoBitmap();

  // Draws a dino image at the center of |canvas|.
  // In the common case where drawing at the same scale as QR modules, note that
  // the QR Code versions from the spec all consist of n*n modules, with n odd,
  // while the dino data is w*h for w,h even, so it will be offset.
  void DrawDino(SkCanvas* canvas,
                const SkRect& canvas_bounds,
                const int pixels_per_dino_tile,
                const int dino_border_px,
                const SkPaint& paint_foreground,
                const SkPaint& paint_background);

  // Draws a passkey icon at the center of |canvas|.
  void DrawPasskeyIcon(SkCanvas* canvas,
                       const SkRect& canvas_bounds,
                       const SkPaint& paint_foreground,
                       const SkPaint& paint_background);

  // Draws |image| at the center of |canvas| with a border of at least
  // |border_px|, snapped to a whole module.
  void PaintCenterImage(SkCanvas* canvas,
                        const SkRect& canvas_bounds,
                        const int width_px,
                        const int height_px,
                        const int border_px,
                        const SkPaint& paint_background,
                        const SkBitmap& image);

  // Renders the QR code with pixel information in |data| and render parameters
  // in |request|.
  // |data| is input data, one element per module, row-major.
  // |data_size| is the dimensions of |data|, in modules. Currently expected to
  //     be square, but function should cope with other shapes.
  // |request| is the mojo service request object to Generate().
  //     It includes rendering style preferences expressed by the client.
  SkBitmap RenderBitmap(base::span<const uint8_t> data,
                        const gfx::Size data_size,
                        const mojom::GenerateQRCodeRequest& request);

  mojo::Receiver<mojom::QRCodeGeneratorService> receiver_;

  SkBitmap dino_bitmap_;
};

}  // namespace qrcode_generator

#endif  // CHROME_SERVICES_QRCODE_GENERATOR_QRCODE_GENERATOR_SERVICE_IMPL_H_
