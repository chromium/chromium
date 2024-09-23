// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "chrome/services/pdf/pdf_thumbnailer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace pdf {

namespace {

constexpr int kThumbWidth = 256;
constexpr int kThumbHeight = 192;
constexpr int kHorizontalDpi = 300;
constexpr int kVerticalDpi = 300;
constexpr bool kStretch = true;
constexpr bool kKeepRatio = true;

const char kPdfContent[] = R"(%PDF-1.2
9 0 obj
<<
>>
stream
BT/ 9 Tf(Test)' ET
endstream
endobj
4 0 obj
<<
/Type /Page
/Parent 5 0 R
/Contents 9 0 R
>>
endobj
5 0 obj
<<
/Kids [4 0 R ]
/Count 1
/Type /Pages
/MediaBox [ 0 0 99 9 ]
>>
endobj
3 0 obj
<<
/Pages 5 0 R
/Type /Catalog
>>
endobj
trailer
<<
/Root 3 0 R
>>
%%EOF)";

}  // namespace

class PdfThumbnailerTest : public testing::Test {
 public:
  void SetUp() override {
    params_ = mojom::ThumbParams::New(gfx::Size(kThumbWidth, kThumbHeight),
                                      gfx::Size(kHorizontalDpi, kVerticalDpi),
                                      kStretch, kKeepRatio);
  }

  // Copies bytes from |bitmap| into |bitmap_|. This method is here due
  // to the fact that GetThumbnail() needs to be run in a single-threaded
  // context. More info at docs/threading_and_tasks_testing.md
  void StoreResult(base::OnceClosure callback, const SkBitmap& bitmap) {
    bitmap_ = std::move(bitmap);
    std::move(callback).Run();
  }

  // Helper method that converts a PDF string to a shared memory region.
  base::ReadOnlySharedMemoryRegion CreatePdfRegion(const std::string& content) {
    auto pdf_region = base::ReadOnlySharedMemoryRegion::Create(content.size());
    EXPECT_TRUE(pdf_region.IsValid());
    memcpy(pdf_region.mapping.memory(), content.data(), content.size());
    return std::move(pdf_region.region);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  PdfThumbnailer thumbnailer_;
  mojom::ThumbParamsPtr params_;
  SkBitmap bitmap_;
  base::WeakPtrFactory<PdfThumbnailerTest> weak_factory_{this};
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// A valid PDF should produce a valid PNG thumbnail.
TEST_F(PdfThumbnailerTest, CreatePdfThumbnail) {
  base::RunLoop run_loop;
  auto pdf_region = CreatePdfRegion(kPdfContent);

  thumbnailer_.GetThumbnail(
      std::move(params_), std::move(pdf_region),
      base::BindOnce(&PdfThumbnailerTest::StoreResult,
                     weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(bitmap_.isNull());
  EXPECT_EQ(kThumbWidth, bitmap_.width());
  EXPECT_EQ(kThumbHeight, bitmap_.height());
}

// A low DPI, non-stretched thumbnail.
TEST_F(PdfThumbnailerTest, CreateLowResUnstretchedPdfThumbnail) {
  base::RunLoop run_loop;
  auto pdf_region = CreatePdfRegion(kPdfContent);

  auto params = mojom::ThumbParams::New(gfx::Size(kThumbWidth, kThumbHeight),
                                        gfx::Size(15, 15), false, kKeepRatio);
  thumbnailer_.GetThumbnail(
      std::move(params), std::move(pdf_region),
      base::BindOnce(&PdfThumbnailerTest::StoreResult,
                     weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(bitmap_.isNull());
  // The bitmap size is still the requested size, even though the thumbnail
  // itself does not fill the entire bitmap.
  EXPECT_EQ(kThumbWidth, bitmap_.width());
  EXPECT_EQ(kThumbHeight, bitmap_.height());
}

// An invalid PDF should cause failure (null bitmap returned).
TEST_F(PdfThumbnailerTest, CreatePdfThumbnailFailure) {
  base::RunLoop run_loop;
  auto pdf_region = CreatePdfRegion("%PDF-1.2 not a valid PDF document");

  thumbnailer_.GetThumbnail(
      std::move(params_), std::move(pdf_region),
      base::BindOnce(&PdfThumbnailerTest::StoreResult,
                     weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(bitmap_.isNull());
}

// An invalid (zero) thumbnail size should cause failure (null bitmap returned).
TEST_F(PdfThumbnailerTest, InvalidThumbnailSize) {
  base::RunLoop run_loop;
  auto pdf_region = CreatePdfRegion(kPdfContent);

  auto params = mojom::ThumbParams::New(gfx::Size(0, 0),
                                        gfx::Size(kHorizontalDpi, kVerticalDpi),
                                        kStretch, kKeepRatio);
  thumbnailer_.GetThumbnail(
      std::move(params), std::move(pdf_region),
      base::BindOnce(&PdfThumbnailerTest::StoreResult,
                     weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(bitmap_.isNull());
}

// A thumbnail size that is larger than maximum should cause failure (null
// bitmap returned).
TEST_F(PdfThumbnailerTest, TooLargeThumbnailSize) {
  base::RunLoop run_loop;
  auto pdf_region = CreatePdfRegion(kPdfContent);

  auto params = mojom::ThumbParams::New(
      gfx::Size(pdf::mojom::PdfThumbnailer::kMaxWidthPixels + 1,
                pdf::mojom::PdfThumbnailer::kMaxHeightPixels + 1),
      gfx::Size(kHorizontalDpi, kVerticalDpi), kStretch, kKeepRatio);
  thumbnailer_.GetThumbnail(
      std::move(params), std::move(pdf_region),
      base::BindOnce(&PdfThumbnailerTest::StoreResult,
                     weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(bitmap_.isNull());
}

TEST_F(PdfThumbnailerTest, CreatePdfThumbnailWithSkiaPolicyEnabled) {
  base::RunLoop run_loop;
  auto pdf_region = CreatePdfRegion(kPdfContent);

  thumbnailer_.SetUseSkiaRendererPolicy(/*use_skia=*/true);
  thumbnailer_.GetThumbnail(
      std::move(params_), std::move(pdf_region),
      base::BindOnce(&PdfThumbnailerTest::StoreResult,
                     weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(bitmap_.isNull());
  EXPECT_EQ(kThumbWidth, bitmap_.width());
  EXPECT_EQ(kThumbHeight, bitmap_.height());
}

TEST_F(PdfThumbnailerTest, CreatePdfThumbnailWithSkiaPolicyDisabled) {
  base::RunLoop run_loop;
  auto pdf_region = CreatePdfRegion(kPdfContent);

  thumbnailer_.SetUseSkiaRendererPolicy(/*use_skia=*/false);
  thumbnailer_.GetThumbnail(
      std::move(params_), std::move(pdf_region),
      base::BindOnce(&PdfThumbnailerTest::StoreResult,
                     weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(bitmap_.isNull());
  EXPECT_EQ(kThumbWidth, bitmap_.width());
  EXPECT_EQ(kThumbHeight, bitmap_.height());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace pdf
