// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/pdf/pdf_progressive_searchifier.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/services/pdf/public/mojom/pdf_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace pdf {

namespace {

class Ocr : public mojom::Ocr {
 public:
  mojo::PendingRemote<mojom::Ocr> CreateRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::Ocr
  void PerformOcr(
      const SkBitmap& image,
      mojom::Ocr::PerformOcrCallback got_annotation_callback) override {
    std::move(got_annotation_callback)
        .Run(screen_ai::mojom::VisualAnnotation::New());
  }

  void Reset() { receiver_.reset(); }

 private:
  mojo::Receiver<mojom::Ocr> receiver_{this};
};

}  // namespace

class PdfProgressiveSearchifierTest : public testing::Test {
 public:
  base::test::SingleThreadTaskEnvironment task_environment_;
  Ocr ocr_;
  PdfProgressiveSearchifier searchifier_{ocr_.CreateRemote()};
};

// All operations on PDF progressive searchifier should run successfully.
TEST_F(PdfProgressiveSearchifierTest, ProgressiveSearchifier) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  searchifier_.AddPage(bitmap, 0);
  searchifier_.AddPage(bitmap, 1);
  searchifier_.DeletePage(0);
  searchifier_.AddPage(bitmap, 0);
  base::RunLoop run_loop;
  std::vector<uint8_t> result_pdf;
  searchifier_.Save(
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& result) {
        result_pdf = result;
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_FALSE(result_pdf.empty());
}

TEST_F(PdfProgressiveSearchifierTest, PerformOcrFailure) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  ocr_.Reset();
  searchifier_.AddPage(bitmap, 0);
}

}  // namespace pdf
