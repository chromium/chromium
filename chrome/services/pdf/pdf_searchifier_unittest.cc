// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/pdf/pdf_searchifier.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/services/pdf/pdf_searchifier.h"
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

 private:
  mojo::Receiver<mojom::Ocr> receiver_{this};
};

}  // namespace

class PdfSearchifierTest : public testing::Test {
 public:
  void SetUp() override {
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
    test_data_dir_ =
        test_data_dir_.AppendASCII("pdf").AppendASCII("accessibility");
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  Ocr ocr_;
  PdfSearchifier searchifier_{ocr_.CreateRemote()};
  base::FilePath test_data_dir_;
};

// A valid PDF with images should run successfully.
TEST_F(PdfSearchifierTest, Searchify) {
  auto pdf =
      base::ReadFileToBytes(test_data_dir_.AppendASCII("image_alt_text.pdf"));
  ASSERT_TRUE(pdf.has_value());
  base::RunLoop run_loop;
  std::vector<uint8_t> result_pdf;
  searchifier_.Searchify(
      *pdf, base::BindLambdaForTesting([&](const std::vector<uint8_t>& result) {
        result_pdf = std::move(result);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_FALSE(result_pdf.empty());
}

}  // namespace pdf
