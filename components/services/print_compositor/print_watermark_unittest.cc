// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/print_compositor/print_watermark.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "cc/test/pixel_test_utils.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/watermarking/mojom/watermark.mojom.h"
#include "components/enterprise/watermarking/watermark.h"
#include "components/enterprise/watermarking/watermark_test_utils.h"
#include "components/services/print_compositor/print_compositor_impl.h"
#include "pdf/pdf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDocument.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/docs/SkMultiPictureDocument.h"
#include "third_party/skia/include/docs/SkPDFDocument.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/skia_span_util.h"

namespace printing {
namespace {

base::ReadOnlySharedMemoryRegion CreateTestPdfRegion(
    base::span<const SkSize> page_sizes) {
  SkDynamicMemoryWStream stream;
  sk_sp<SkDocument> doc = SkPDF::MakeDocument(&stream);
  for (const auto& size : page_sizes) {
    SkCanvas* canvas = doc->beginPage(size.width(), size.height());
    if (!canvas) {
      return base::ReadOnlySharedMemoryRegion();
    }
    // Uses a solid black background because MakeTestWatermarkBlock() creates
    // white watermark text (consistent with OnDrawPage testing).
    canvas->clear(SK_ColorBLACK);
    doc->endPage();
  }
  doc->close();

  sk_sp<SkData> data = stream.detachAsData();
  if (data->isEmpty()) {
    return base::ReadOnlySharedMemoryRegion();
  }

  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(data->size());
  if (!mapped_region.IsValid()) {
    return base::ReadOnlySharedMemoryRegion();
  }

  mapped_region.mapping.GetMemoryAsSpan<uint8_t>()
      .first(data->size())
      .copy_from(gfx::SkDataToSpan(data));

  return std::move(mapped_region.region);
}

constexpr SkSize kWatermarkSize{200, 200};
constexpr char kWatermarkText[] = "example-watermark";

class MockPrintCompositorImplEnterpriseWatermark : public PrintCompositorImpl {
 public:
  MockPrintCompositorImplEnterpriseWatermark()
      : PrintCompositorImpl(mojo::NullReceiver(),
                            /*initialize_environment=*/false,
                            /*io_task_runner=*/nullptr) {
    SetWatermarkBlock(enterprise_watermark::MakeTestWatermarkBlock(
        kWatermarkText, kWatermarkSize));
  }

  ~MockPrintCompositorImplEnterpriseWatermark() override = default;

  void DrawPage(SkDocument* doc, const SkDocumentPage& page) override {
    bitmap_.allocN32Pixels(kWatermarkSize.fWidth, kWatermarkSize.fHeight);
    SkCanvas canvas(bitmap_);
    canvas.clear(SK_ColorBLACK);
    PrintWatermark* watermark = watermark_for_testing();
    ASSERT_TRUE(watermark);
    watermark->OnDrawPage(&canvas, kWatermarkSize);
  }

  const SkBitmap& bitmap() const { return bitmap_; }

 private:
  SkBitmap bitmap_;
};

class PrintWatermarkTest : public testing::Test {
 public:
  PrintWatermarkTest() {
    // Create reference bitmap.
    reference_watermark_.allocN32Pixels(kWatermarkSize.fWidth,
                                        kWatermarkSize.fHeight);
    SkCanvas canvas(reference_watermark_);
    canvas.clear(SK_ColorBLACK);
    auto watermark =
        PrintWatermark::Create(enterprise_watermark::MakeTestWatermarkBlock(
            kWatermarkText, kWatermarkSize));
    CHECK(watermark);
    watermark->OnDrawPage(&canvas, kWatermarkSize);
  }

  const SkBitmap& reference_watermark() const { return reference_watermark_; }

 protected:
  // Required by PDFium SDK initialization (which expects a
  // SingleThreadTaskRunner).
  base::test::TaskEnvironment task_environment_;
  SkBitmap reference_watermark_;
};

TEST_F(PrintWatermarkTest, EnterpriseWatermarkSet) {
  MockPrintCompositorImplEnterpriseWatermark compositor;
  compositor.DrawPage(nullptr, {});

  ASSERT_TRUE(cc::MatchesBitmap(compositor.bitmap(), reference_watermark(),
                                cc::ExactPixelComparator()));
}

TEST_F(PrintWatermarkTest, EnterpriseWatermarkUnset) {
  MockPrintCompositorImplEnterpriseWatermark compositor;
  EXPECT_NE(compositor.watermark_for_testing(), nullptr);

  compositor.SetWatermarkBlock(nullptr);
  EXPECT_EQ(compositor.watermark_for_testing(), nullptr);
}

TEST_F(PrintWatermarkTest, OnOverlayPdfSuccess) {
  auto watermark =
      PrintWatermark::Create(enterprise_watermark::MakeTestWatermarkBlock(
          kWatermarkText, kWatermarkSize));
  ASSERT_TRUE(watermark);

  constexpr SkSize kPages[] = {SkSize::Make(100, 150), SkSize::Make(200, 300)};
  base::ReadOnlySharedMemoryRegion input_region = CreateTestPdfRegion(kPages);
  ASSERT_TRUE(input_region.IsValid());

  base::ReadOnlySharedMemoryMapping input_mapping = input_region.Map();
  ASSERT_TRUE(input_mapping.IsValid());

  base::ReadOnlySharedMemoryRegion output_region =
      watermark->OnOverlayPdf(std::move(input_region));
  ASSERT_TRUE(output_region.IsValid());

  base::ReadOnlySharedMemoryMapping mapping = output_region.Map();
  ASSERT_TRUE(mapping.IsValid());

  int page_count = 0;
  ASSERT_TRUE(chrome_pdf::GetPDFDocInfo(
      mapping.GetMemoryAsSpan<const uint8_t>(), &page_count, nullptr));
  EXPECT_EQ(page_count, 2);

  EXPECT_EQ(chrome_pdf::GetPDFPageSizeByIndex(
                mapping.GetMemoryAsSpan<const uint8_t>(), 0),
            gfx::SizeF(100, 150));
  EXPECT_EQ(chrome_pdf::GetPDFPageSizeByIndex(
                mapping.GetMemoryAsSpan<const uint8_t>(), 1),
            gfx::SizeF(200, 300));

  chrome_pdf::RenderOptions options{
      .stretch_to_bounds = true,
      .keep_aspect_ratio = true,
      .autorotate = false,
      .use_color = true,
      .render_device_type = chrome_pdf::RenderDeviceType::kDisplay};

  // Render page 0 of the input PDF (before watermarking).
  SkBitmap input_bitmap;
  input_bitmap.allocN32Pixels(100, 150);
  ASSERT_TRUE(chrome_pdf::RenderPDFPageToBitmap(
      input_mapping.GetMemoryAsSpan<const uint8_t>(), /*page_index=*/0,
      input_bitmap.getPixels(), gfx::Size(100, 150), gfx::Size(72, 72),
      options));

  // Render page 0 of the output watermarked PDF.
  SkBitmap output_bitmap;
  output_bitmap.allocN32Pixels(100, 150);
  ASSERT_TRUE(chrome_pdf::RenderPDFPageToBitmap(
      mapping.GetMemoryAsSpan<const uint8_t>(), /*page_index=*/0,
      output_bitmap.getPixels(), gfx::Size(100, 150), gfx::Size(72, 72),
      options));

#if BUILDFLAG(IS_WIN)
  const char kFileName[] = "watermark_pdf_overlay_win.png";
#elif BUILDFLAG(IS_MAC)
  const char kFileName[] = "watermark_pdf_overlay_mac.png";
#else
  const char kFileName[] = "watermark_pdf_overlay.png";
#endif

  base::FilePath path =
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
          .AppendASCII("components")
          .AppendASCII("test")
          .AppendASCII("data")
          .AppendASCII("enterprise")
          .AppendASCII(kFileName);
  EXPECT_TRUE(
      cc::MatchesPNGFile(output_bitmap, path, cc::ExactPixelComparator()));
}

TEST_F(PrintWatermarkTest, CreateWithCorruptedWatermarkBlockFails) {
  auto corrupt_pic_data =
      base::byte_span_with_nul_from_cstring("Invalid SkPicture stream");
  base::MappedReadOnlyRegion mapped_pic =
      base::ReadOnlySharedMemoryRegion::Create(corrupt_pic_data.size());
  ASSERT_TRUE(mapped_pic.IsValid());
  mapped_pic.mapping.GetMemoryAsSpan<uint8_t>().copy_from(corrupt_pic_data);

  auto bad_block = watermark::mojom::WatermarkBlock::New();
  bad_block->width = kWatermarkSize.width();
  bad_block->height = kWatermarkSize.height();
  bad_block->serialized_skpicture = std::move(mapped_pic.region);

  EXPECT_FALSE(PrintWatermark::Create(std::move(bad_block)));
}

TEST_F(PrintWatermarkTest, CreateWithInvalidRegionFails) {
  auto bad_block = watermark::mojom::WatermarkBlock::New();
  bad_block->width = kWatermarkSize.width();
  bad_block->height = kWatermarkSize.height();

  EXPECT_FALSE(PrintWatermark::Create(std::move(bad_block)));
}

TEST_F(PrintWatermarkTest, OnOverlayPdfInvalidInputRegion) {
  auto watermark =
      PrintWatermark::Create(enterprise_watermark::MakeTestWatermarkBlock(
          kWatermarkText, kWatermarkSize));
  ASSERT_TRUE(watermark);

  base::ReadOnlySharedMemoryRegion output_region =
      watermark->OnOverlayPdf(base::ReadOnlySharedMemoryRegion());
  EXPECT_FALSE(output_region.IsValid());
}

TEST_F(PrintWatermarkTest, OnOverlayPdfCorruptedPdf) {
  auto watermark =
      PrintWatermark::Create(enterprise_watermark::MakeTestWatermarkBlock(
          kWatermarkText, kWatermarkSize));
  ASSERT_TRUE(watermark);

  auto corrupt_data =
      base::byte_span_with_nul_from_cstring("Invalid PDF data stream");
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(corrupt_data.size());
  ASSERT_TRUE(mapped_region.IsValid());
  mapped_region.mapping.GetMemoryAsSpan<uint8_t>().copy_from(corrupt_data);

  base::ReadOnlySharedMemoryRegion output_region =
      watermark->OnOverlayPdf(std::move(mapped_region.region));
  EXPECT_FALSE(output_region.IsValid());
}

TEST_F(PrintWatermarkTest, EnterpriseWatermarkInitFailureFailsPdfComposition) {
  PrintCompositorImpl compositor(mojo::NullReceiver(),
                                 /*initialize_environment=*/false,
                                 /*io_task_runner=*/nullptr);

  auto bad_block = watermark::mojom::WatermarkBlock::New();
  bad_block->width = kWatermarkSize.width();
  bad_block->height = kWatermarkSize.height();
  compositor.SetWatermarkBlock(std::move(bad_block));
  EXPECT_FALSE(compositor.watermark_for_testing());

  base::test::TestFuture<mojom::PrintCompositor::Status,
                         base::ReadOnlySharedMemoryRegion>
      future;
  compositor.CompositeDocument(1, base::ReadOnlySharedMemoryRegion(),
                               /*is_pdf=*/true, ContentToFrameMap(),
                               future.GetCallback());
  EXPECT_EQ(future.Get<0>(),
            mojom::PrintCompositor::Status::kContentFormatError);
  EXPECT_FALSE(future.Get<1>().IsValid());
}

TEST_F(PrintWatermarkTest, EnterpriseWatermarkInitFailureFailsHtmlComposition) {
  PrintCompositorImpl compositor(mojo::NullReceiver(),
                                 /*initialize_environment=*/false,
                                 /*io_task_runner=*/nullptr);

  auto bad_block = watermark::mojom::WatermarkBlock::New();
  bad_block->width = kWatermarkSize.width();
  bad_block->height = kWatermarkSize.height();
  compositor.SetWatermarkBlock(std::move(bad_block));
  EXPECT_FALSE(compositor.watermark_for_testing());

  base::test::TestFuture<mojom::PrintCompositor::Status> prepare_future;
  compositor.PrepareToCompositeDocument(prepare_future.GetCallback());
  EXPECT_EQ(prepare_future.Get(),
            mojom::PrintCompositor::Status::kContentFormatError);

  base::test::TestFuture<mojom::PrintCompositor::Status,
                         base::ReadOnlySharedMemoryRegion>
      composite_future;
  compositor.CompositeDocument(1, base::ReadOnlySharedMemoryRegion(),
                               /*is_pdf=*/false, ContentToFrameMap(),
                               composite_future.GetCallback());
  EXPECT_EQ(composite_future.Get<0>(),
            mojom::PrintCompositor::Status::kContentFormatError);
  EXPECT_FALSE(composite_future.Get<1>().IsValid());
}

}  // namespace
}  // namespace printing
