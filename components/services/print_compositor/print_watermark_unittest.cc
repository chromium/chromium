// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/print_compositor/print_watermark.h"

#include "base/task/single_thread_task_runner.h"
#include "cc/test/pixel_test_utils.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/watermarking/mojom/watermark.mojom.h"
#include "components/enterprise/watermarking/watermark.h"
#include "components/enterprise/watermarking/watermark_test_utils.h"
#include "components/services/print_compositor/print_compositor_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/docs/SkMultiPictureDocument.h"

namespace printing {
namespace {

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
    PrintWatermark watermark(enterprise_watermark::MakeTestWatermarkBlock(
        kWatermarkText, kWatermarkSize));
    watermark.OnDrawPage(&canvas, kWatermarkSize);
  }

  const SkBitmap& reference_watermark() const { return reference_watermark_; }

 protected:
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

}  // namespace
}  // namespace printing
