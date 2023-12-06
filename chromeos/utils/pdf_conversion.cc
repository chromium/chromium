// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/utils/pdf_conversion.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "printing/units.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/codec/SkJpegDecoder.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"
#include "ui/gfx/image/buffer_w_stream.h"

namespace chromeos {

namespace {

// The number of degrees to rotate a PDF image.
constexpr int kRotationDegrees = 180;

// Creates a new page for the PDF document and adds `image_data` to the page.
// `rotate` indicates whether the page should be rotated 180 degrees.
// Returns whether the page was successfully created.
bool AddPdfPage(sk_sp<SkDocument> pdf_doc,
                const sk_sp<SkData>& jpeg_image_data,
                bool rotate,
                std::optional<int> dpi) {
  if (!SkJpegDecoder::IsJpeg(jpeg_image_data->data(),
                             jpeg_image_data->size())) {
    LOG(ERROR) << "Not a valid JPEG image.";
    return false;
  }
  CHECK(
      SkJpegDecoder::IsJpeg(jpeg_image_data->data(), jpeg_image_data->size()));
  const sk_sp<SkImage> image =
      SkImages::DeferredFromEncodedData(jpeg_image_data);
  if (!image) {
    LOG(ERROR) << "Unable to generate image from encoded image data.";
    return false;
  }

  // Convert from JPG dimensions in pixels (DPI) to PDF dimensions in points
  // (1/72 in).
  int page_width;
  int page_height;
  if (dpi.has_value() && dpi.value() > 0) {
    page_width = printing::ConvertUnit(image->width(), dpi.value(),
                                       printing::kPointsPerInch);
    page_height = printing::ConvertUnit(image->height(), dpi.value(),
                                        printing::kPointsPerInch);
  } else {
    page_width = image->width();
    page_height = image->height();
  }
  SkCanvas* page_canvas = pdf_doc->beginPage(page_width, page_height);
  if (!page_canvas) {
    LOG(ERROR) << "Unable to access PDF page canvas.";
    return false;
  }

  // Rotate pages that were flipped by an ADF scanner.
  if (rotate) {
    page_canvas->rotate(kRotationDegrees);
    page_canvas->translate(-page_width, -page_height);
  }

  SkRect image_bounds = SkRect::MakeIWH(page_width, page_height);
  page_canvas->drawImageRect(image, image_bounds, SkSamplingOptions());
  pdf_doc->endPage();
  return true;
}

}  // namespace

bool ConvertJpgImagesToPdf(const std::vector<std::string>& jpg_images,
                           const base::FilePath& file_path,
                           bool rotate_alternate_pages,
                           std::optional<int> dpi) {
  DCHECK(!file_path.empty());

  // Register Jpeg Decoder for use by DeferredFromEncodedData in AddPdfPage.
  SkCodecs::Register(SkJpegDecoder::Decoder());

  SkFILEWStream pdf_outfile(file_path.value().c_str());
  if (!pdf_outfile.isValid()) {
    LOG(ERROR) << "Unable to open output file.";
    return false;
  }

  sk_sp<SkDocument> pdf_doc = SkPDF::MakeDocument(&pdf_outfile);
  DCHECK(pdf_doc);

  // Never rotate first page of PDF.
  bool rotate_current_page = false;
  for (const auto& jpg_image : jpg_images) {
    SkDynamicMemoryWStream img_stream;
    if (!img_stream.write(jpg_image.c_str(), jpg_image.size())) {
      LOG(ERROR) << "Unable to write image to dynamic memory stream.";
      return false;
    }

    const sk_sp<SkData> img_data = img_stream.detachAsData();
    if (img_data->isEmpty()) {
      LOG(ERROR) << "Stream data is empty.";
      return false;
    }

    if (!AddPdfPage(pdf_doc, img_data, rotate_current_page, dpi)) {
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

bool ConvertJpgImagesToPdf(const std::vector<std::vector<uint8_t>>& jpg_images,
                           std::vector<uint8_t>* output) {
  gfx::BufferWStream output_stream;
  sk_sp<SkDocument> pdf_doc = SkPDF::MakeDocument(&output_stream);
  DCHECK(pdf_doc);

  // Register Jpeg Decoder for use by DeferredFromEncodedData in AddPdfPage.
  SkCodecs::Register(SkJpegDecoder::Decoder());

  for (const auto& jpg_image : jpg_images) {
    SkDynamicMemoryWStream img_stream;
    bool result = img_stream.write(jpg_image.data(), jpg_image.size());
    CHECK(result);

    const sk_sp<SkData> img_data = img_stream.detachAsData();
    if (img_data->isEmpty()) {
      LOG(ERROR) << "Stream data is empty.";
      return false;
    }

    if (!AddPdfPage(pdf_doc, img_data, false, std::nullopt)) {
      LOG(ERROR) << "Unable to add new PDF page.";
      return false;
    }
  }

  pdf_doc->close();
  *output = output_stream.TakeBuffer();
  return true;
}

}  // namespace chromeos
