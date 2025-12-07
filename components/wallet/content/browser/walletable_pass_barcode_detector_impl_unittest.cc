// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/browser/walletable_pass_barcode_detector_impl.h"

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/wallet/content/browser/walletable_pass_barcode_detector.h"
#include "components/wallet/content/common/mojom/image_extractor.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace wallet {
namespace {

using testing::ElementsAre;

shape_detection::mojom::BarcodeDetectionResultPtr CreateBarcodeDetectionResult(
    const std::string& raw_value,
    shape_detection::mojom::BarcodeFormat format) {
  auto barcode = shape_detection::mojom::BarcodeDetectionResult::New();
  barcode->raw_value = raw_value;
  barcode->format = format;
  return barcode;
}

class MockImageExtractor : public mojom::ImageExtractor {
 public:
  MockImageExtractor() = default;
  ~MockImageExtractor() override = default;

  void Bind(mojo::PendingReceiver<mojom::ImageExtractor> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD(void,
              ExtractImages,
              (ExtractImagesCallback callback),
              (override));

  void Disconnect() { receiver_.reset(); }

 private:
  mojo::Receiver<mojom::ImageExtractor> receiver_{this};
};

class MockBarcodeDetector : public shape_detection::mojom::BarcodeDetection {
 public:
  MockBarcodeDetector() = default;
  ~MockBarcodeDetector() override = default;

  void Bind(mojo::PendingReceiver<shape_detection::mojom::BarcodeDetection>
                receiver) {
    receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD(void,
              Detect,
              (const SkBitmap& bitmap_data, DetectCallback callback),
              (override));

  void Disconnect() { receiver_.reset(); }

 private:
  mojo::Receiver<shape_detection::mojom::BarcodeDetection> receiver_{this};
};

// A test class that overrides the mojo remote creation methods to allow for
// dependency injection of mock objects.
class TestWalletablePassBarcodeDetectorImpl
    : public WalletablePassBarcodeDetectorImpl {
 public:
  TestWalletablePassBarcodeDetectorImpl() = default;
  ~TestWalletablePassBarcodeDetectorImpl() override = default;

  mojo::Remote<mojom::ImageExtractor> CreateAndBindImageExtractorRemote(
      content::WebContents* web_contents) override {
    mojo::Remote<mojom::ImageExtractor> remote;
    image_extractor_.Bind(remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  mojo::Remote<shape_detection::mojom::BarcodeDetection>
  CreateAndBindBarcodeDetectorRemote() override {
    mojo::Remote<shape_detection::mojom::BarcodeDetection> remote;
    barcode_detector_.Bind(remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  MockImageExtractor& image_extractor() { return image_extractor_; }
  MockBarcodeDetector& barcode_detector() { return barcode_detector_; }

 private:
  MockImageExtractor image_extractor_;
  MockBarcodeDetector barcode_detector_;
};

class WalletablePassBarcodeDetectorImplTest
    : public content::RenderViewHostTestHarness {
 public:
  WalletablePassBarcodeDetectorImplTest() = default;

  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }

  void TearDown() override { content::RenderViewHostTestHarness::TearDown(); }

 protected:
  TestWalletablePassBarcodeDetectorImpl detector_;
};

TEST_F(WalletablePassBarcodeDetectorImplTest, NoImagesFound) {
  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce([](mojom::ImageExtractor::ExtractImagesCallback callback) {
        std::move(callback).Run({});
      });

  base::RunLoop run_loop;
  detector_.Detect(web_contents(),
                   base::BindLambdaForTesting(
                       [&](const std::vector<WalletBarcode>& results) {
                         EXPECT_TRUE(results.empty());
                         run_loop.Quit();
                       }));
  run_loop.Run();
}

TEST_F(WalletablePassBarcodeDetectorImplTest, NoBarcodesFound) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  std::vector<SkBitmap> images = {bitmap};

  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce(
          [images](mojom::ImageExtractor::ExtractImagesCallback callback) {
            std::move(callback).Run(images);
          });

  EXPECT_CALL(detector_.barcode_detector(), Detect)
      .WillOnce([](const SkBitmap&,
                   shape_detection::mojom::BarcodeDetection::DetectCallback
                       callback) { std::move(callback).Run({}); });

  base::RunLoop run_loop;
  detector_.Detect(web_contents(),
                   base::BindLambdaForTesting(
                       [&](const std::vector<WalletBarcode>& results) {
                         EXPECT_TRUE(results.empty());
                         run_loop.Quit();
                       }));
  run_loop.Run();
}

TEST_F(WalletablePassBarcodeDetectorImplTest, QRCodeFound) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  std::vector<SkBitmap> images = {bitmap};

  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce(
          [images](mojom::ImageExtractor::ExtractImagesCallback callback) {
            std::move(callback).Run(images);
          });

  std::vector<shape_detection::mojom::BarcodeDetectionResultPtr> barcodes;
  barcodes.push_back(CreateBarcodeDetectionResult(
      "test_value", shape_detection::mojom::BarcodeFormat::QR_CODE));

  EXPECT_CALL(detector_.barcode_detector(), Detect)
      .WillOnce(
          [&barcodes](const SkBitmap&,
                      shape_detection::mojom::BarcodeDetection::DetectCallback
                          callback) {
            std::move(callback).Run(std::move(barcodes));
          });

  base::RunLoop run_loop;
  detector_.Detect(
      web_contents(),
      base::BindLambdaForTesting(
          [&](const std::vector<WalletBarcode>& results) {
            EXPECT_THAT(results, ElementsAre(WalletBarcode{
                                     .raw_value = "test_value",
                                     .format = WalletBarcodeFormat::QR_CODE}));
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(WalletablePassBarcodeDetectorImplTest,
       ImageExtractorRemoteDisconnected) {
  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce([&](mojom::ImageExtractor::ExtractImagesCallback) {
        detector_.image_extractor().Disconnect();
      });

  base::RunLoop run_loop;
  detector_.Detect(web_contents(),
                   base::BindLambdaForTesting(
                       [&](const std::vector<WalletBarcode>& results) {
                         EXPECT_TRUE(results.empty());
                         run_loop.Quit();
                       }));
  run_loop.Run();
}

TEST_F(WalletablePassBarcodeDetectorImplTest,
       BarcodeDetectorRemoteDisconnected) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  std::vector<SkBitmap> images = {bitmap};

  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce(
          [images](mojom::ImageExtractor::ExtractImagesCallback callback) {
            std::move(callback).Run(images);
          });

  EXPECT_CALL(detector_.barcode_detector(), Detect)
      .WillOnce([&](const SkBitmap&,
                    shape_detection::mojom::BarcodeDetection::DetectCallback) {
        detector_.barcode_detector().Disconnect();
      });

  base::RunLoop run_loop;
  detector_.Detect(web_contents(),
                   base::BindLambdaForTesting(
                       [&](const std::vector<WalletBarcode>& results) {
                         EXPECT_TRUE(results.empty());
                         run_loop.Quit();
                       }));
  run_loop.Run();
}

TEST_F(WalletablePassBarcodeDetectorImplTest, MultipleBarcodesFound) {
  SkBitmap bitmap1;
  bitmap1.allocN32Pixels(10, 10);
  SkBitmap bitmap2;
  bitmap2.allocN32Pixels(10, 10);
  std::vector<SkBitmap> images = {bitmap1, bitmap2};

  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce(
          [images](mojom::ImageExtractor::ExtractImagesCallback callback) {
            std::move(callback).Run(images);
          });

  EXPECT_CALL(detector_.barcode_detector(), Detect)
      .WillOnce([](const SkBitmap&,
                   shape_detection::mojom::BarcodeDetection::DetectCallback
                       callback) {
        std::vector<shape_detection::mojom::BarcodeDetectionResultPtr> barcodes;
        barcodes.push_back(CreateBarcodeDetectionResult(
            "qr_code_value", shape_detection::mojom::BarcodeFormat::QR_CODE));
        std::move(callback).Run(std::move(barcodes));
      })
      .WillOnce([](const SkBitmap&,
                   shape_detection::mojom::BarcodeDetection::DetectCallback
                       callback) {
        std::vector<shape_detection::mojom::BarcodeDetectionResultPtr> barcodes;
        barcodes.push_back(CreateBarcodeDetectionResult(
            "pdf417_value", shape_detection::mojom::BarcodeFormat::PDF417));
        std::move(callback).Run(std::move(barcodes));
      });

  base::RunLoop run_loop;
  detector_.Detect(
      web_contents(),
      base::BindLambdaForTesting(
          [&](const std::vector<WalletBarcode>& results) {
            EXPECT_THAT(
                results,
                ElementsAre(
                    WalletBarcode{.raw_value = "qr_code_value",
                                  .format = WalletBarcodeFormat::QR_CODE},
                    WalletBarcode{.raw_value = "pdf417_value",
                                  .format = WalletBarcodeFormat::PDF417}));
            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace
}  // namespace wallet
