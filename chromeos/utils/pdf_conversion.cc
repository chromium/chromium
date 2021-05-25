// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/utils/pdf_conversion.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"
#include "ui/gfx/image/buffer_w_stream.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

namespace chromeos {

namespace {

// The number of degrees to rotate a PDF image.
constexpr int kRotationDegrees = 180;

// Converts `png_img` to JPG.
std::vector<uint8_t> PngToJpg(const uint8_t* data,
                              size_t size,
                              int jpg_quality) {
  std::vector<uint8_t> jpg_img;
  const gfx::Image img = gfx::Image::CreateFrom1xPNGBytes(
      reinterpret_cast<const uint8_t*>(data), size);
  if (!gfx::JPEG1xEncodedDataFromImage(img, jpg_quality, &jpg_img)) {
    LOG(ERROR) << "Failed to convert image from PNG to JPG.";
    return {};
  }
  return jpg_img;
}

// Creates a new page for the PDF document and adds `image_data` to the page.
// `rotate` indicates whether the page should be rotated 180 degrees.
// Returns whether the page was successfully created.
bool AddPdfPage(sk_sp<SkDocument> pdf_doc,
                const sk_sp<SkData>& image_data,
                bool rotate) {
  const sk_sp<SkImage> image = SkImage::MakeFromEncoded(image_data);
  if (!image) {
    LOG(ERROR) << "Unable to generate image from encoded image data.";
    return false;
  }

  SkCanvas* page_canvas = pdf_doc->beginPage(image->width(), image->height());
  if (!page_canvas) {
    LOG(ERROR) << "Unable to access PDF page canvas.";
    return false;
  }

  // Rotate pages that were flipped by an ADF scanner.
  if (rotate) {
    page_canvas->rotate(kRotationDegrees);
    page_canvas->translate(-image->width(), -image->height());
  }

  page_canvas->drawImage(image, /*left=*/0, /*top=*/0);
  pdf_doc->endPage();
  return true;
}

}  // namespace

bool ConvertPngImagesToPdf(const std::vector<std::string>& png_images,
                           const base::FilePath& file_path,
                           bool rotate_alternate_pages,
                           int jpg_quality) {
  DCHECK(!file_path.empty());

  SkFILEWStream pdf_outfile(file_path.value().c_str());
  if (!pdf_outfile.isValid()) {
    LOG(ERROR) << "Unable to open output file.";
    return false;
  }

  sk_sp<SkDocument> pdf_doc = SkPDF::MakeDocument(&pdf_outfile);
  DCHECK(pdf_doc);

  // Never rotate first page of PDF.
  bool rotate_current_page = false;
  for (const auto& png_image : png_images) {
    SkDynamicMemoryWStream img_stream;
    auto jpg_buffer =
        PngToJpg(reinterpret_cast<const uint8_t*>(png_image.c_str()),
                 png_image.size(), jpg_quality);
    if (!img_stream.write(jpg_buffer.data(), jpg_buffer.size())) {
      LOG(ERROR) << "Unable to write image to dynamic memory stream.";
      return false;
    }

    const sk_sp<SkData> img_data = img_stream.detachAsData();
    if (img_data->isEmpty()) {
      LOG(ERROR) << "Stream data is empty.";
      return false;
    }

    if (!AddPdfPage(pdf_doc, img_data, rotate_current_page)) {
      LOG(ERROR) << "Unable to add new PDF page.";
      return false;
    }

    if (rotate_alternate_pages) {
      rotate_current_page = !rotate_current_page;
    }
  }

  pdf_doc->close();
  return true;
}

bool ConvertJpgImageToPdf(const std::vector<uint8_t>& jpg_image,
                          std::vector<uint8_t>* output) {
  gfx::BufferWStream output_stream;
  sk_sp<SkDocument> pdf_doc = SkPDF::MakeDocument(&output_stream);
  DCHECK(pdf_doc);

  SkDynamicMemoryWStream img_stream;
  if (!img_stream.write(jpg_image.data(), jpg_image.size())) {
    LOG(ERROR) << "Unable to write image to dynamic memory stream.";
    return false;
  }

  const sk_sp<SkData> img_data = img_stream.detachAsData();
  if (img_data->isEmpty()) {
    LOG(ERROR) << "Stream data is empty.";
    return false;
  }

  if (!AddPdfPage(pdf_doc, img_data, false)) {
    LOG(ERROR) << "Unable to add new PDF page.";
    return false;
  }

  pdf_doc->close();
  *output = output_stream.TakeBuffer();
  return true;
}

}  // namespace chromeos
