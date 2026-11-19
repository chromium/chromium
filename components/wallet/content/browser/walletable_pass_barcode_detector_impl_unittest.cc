// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/browser/walletable_pass_barcode_detector_impl.h"

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/wallet/content/browser/walletable_pass_barcode_detector.h"
#include "components/wallet/content/common/mojom/image_extractor.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/shape_detection/public/mojom/barcodedetection.mojom.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace wallet {
namespace {

using testing::ElementsAre;
using testing::UnorderedElementsAre;
using BarcodeDetectionStatus =
    WalletablePassBarcodeDetector::BarcodeDetectionStatus;

shape_detection::mojom::BarcodeDetectionResultPtr CreateBarcodeDetectionResult(
    const std::string& raw_value,
    shape_detection::mojom::BarcodeFormat format) {
  auto barcode = shape_detection::mojom::BarcodeDetectionResult::New();
  barcode->raw_value = raw_value;
  barcode->format = format;
  return barcode;
}

// Any valid options; these tests don't depend on the filter values.
mojom::ImageExtractionOptionsPtr TestOptions() {
  static constexpr uint32_t kMinImageSize = 10;
  static constexpr double kMaxAspectRatio = 15.0;
  return mojom::ImageExtractionOptions::New(kMinImageSize, kMaxAspectRatio,
                                            /*include_canvas=*/false);
}

// Returns one small image for each of `source_elements`.
std::vector<mojom::ExtractedImagePtr> MakeImages(
    std::vector<mojom::ImageSourceElement> source_elements) {
  static constexpr int kImageSize = 10;
  std::vector<mojom::ExtractedImagePtr> images;
  for (mojom::ImageSourceElement source_element : source_elements) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(kImageSize, kImageSize);
    images.push_back(
        mojom::ExtractedImage::New(std::move(bitmap), source_element));
  }
  return images;
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
              (mojom::ImageExtractionOptionsPtr options,
               ExtractImagesCallback callback),
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
  CreateAndBindBarcodeDetectorRemote(
      shape_detection::mojom::BarcodeDetectorOptionsPtr options) override {
    last_barcode_formats_ = options->formats;
    mojo::Remote<shape_detection::mojom::BarcodeDetection> remote;
    barcode_detector_.Bind(remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  MockImageExtractor& image_extractor() { return image_extractor_; }
  MockBarcodeDetector& barcode_detector() { return barcode_detector_; }
  const std::vector<shape_detection::mojom::BarcodeFormat>&
  last_barcode_formats() const {
    return last_barcode_formats_;
  }

 private:
  MockImageExtractor image_extractor_;
  MockBarcodeDetector barcode_detector_;
  std::vector<shape_detection::mojom::BarcodeFormat> last_barcode_formats_;
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
      .WillOnce([](mojom::ImageExtractionOptionsPtr,
                   mojom::ImageExtractor::ExtractImagesCallback callback) {
        std::move(callback).Run({});
      });

  base::RunLoop run_loop;
  detector_.Detect(
      web_contents(), TestOptions(), /*barcode_formats=*/{},
      base::BindLambdaForTesting([&](BarcodeDetectionStatus status,
                                     std::vector<WalletBarcode> results) {
        EXPECT_EQ(status, BarcodeDetectionStatus::kImagesNotFound);
        EXPECT_TRUE(results.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Tests that the caller's extraction options are forwarded to the renderer.
TEST_F(WalletablePassBarcodeDetectorImplTest, ForwardsExtractionOptions) {
  static constexpr uint32_t kMinImageSize = 50;
  static constexpr double kMaxAspectRatio = 1.2;
  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce([](mojom::ImageExtractionOptionsPtr options,
                   mojom::ImageExtractor::ExtractImagesCallback callback) {
        EXPECT_EQ(kMinImageSize, options->min_image_size);
        EXPECT_EQ(kMaxAspectRatio, options->max_aspect_ratio);
        EXPECT_TRUE(options->include_canvas);
        std::move(callback).Run({});
      });

  base::RunLoop run_loop;
  detector_.Detect(
      web_contents(),
      mojom::ImageExtractionOptions::New(kMinImageSize, kMaxAspectRatio,
                                         /*include_canvas=*/true),
      /*barcode_formats=*/{},
      base::BindLambdaForTesting([&](BarcodeDetectionStatus status,
                                     std::vector<WalletBarcode> results) {
        EXPECT_EQ(status, BarcodeDetectionStatus::kImagesNotFound);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Tests that the caller's barcode formats are forwarded to the barcode
// detection service.
TEST_F(WalletablePassBarcodeDetectorImplTest, ForwardsBarcodeFormats) {
  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce([](mojom::ImageExtractionOptionsPtr,
                   mojom::ImageExtractor::ExtractImagesCallback callback) {
        std::move(callback).Run(MakeImages({mojom::ImageSourceElement::kImg}));
      });
  EXPECT_CALL(detector_.barcode_detector(), Detect)
      .WillOnce([](const SkBitmap&,
                   shape_detection::mojom::BarcodeDetection::DetectCallback
                       callback) { std::move(callback).Run({}); });

  base::RunLoop run_loop;
  detector_.Detect(
      web_contents(), TestOptions(),
      {shape_detection::mojom::BarcodeFormat::QR_CODE},
      base::BindLambdaForTesting([&](BarcodeDetectionStatus status,
                                     std::vector<WalletBarcode> results) {
        EXPECT_EQ(status, BarcodeDetectionStatus::kSuccess);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_THAT(detector_.last_barcode_formats(),
              ElementsAre(shape_detection::mojom::BarcodeFormat::QR_CODE));
}

// Tests that `UNKNOWN` is never requested from the barcode detection service,
// since it cannot be decoded and the Mac detector rejects it.
TEST_F(WalletablePassBarcodeDetectorImplTest, DropsUnknownBarcodeFormat) {
  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce([](mojom::ImageExtractionOptionsPtr,
                   mojom::ImageExtractor::ExtractImagesCallback callback) {
        std::move(callback).Run(MakeImages({mojom::ImageSourceElement::kImg}));
      });
  EXPECT_CALL(detector_.barcode_detector(), Detect)
      .WillOnce([](const SkBitmap&,
                   shape_detection::mojom::BarcodeDetection::DetectCallback
                       callback) { std::move(callback).Run({}); });

  base::RunLoop run_loop;
  detector_.Detect(
      web_contents(), TestOptions(),
      {shape_detection::mojom::BarcodeFormat::UNKNOWN,
       shape_detection::mojom::BarcodeFormat::QR_CODE},
      base::BindLambdaForTesting([&](BarcodeDetectionStatus status,
                                     std::vector<WalletBarcode> results) {
        EXPECT_EQ(status, BarcodeDetectionStatus::kSuccess);
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_THAT(detector_.last_barcode_formats(),
              ElementsAre(shape_detection::mojom::BarcodeFormat::QR_CODE));
}

TEST_F(WalletablePassBarcodeDetectorImplTest, NoBarcodesFound) {
  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce([](mojom::ImageExtractionOptionsPtr,
                   mojom::ImageExtractor::ExtractImagesCallback callback) {
        std::move(callback).Run(MakeImages({mojom::ImageSourceElement::kImg}));
      });

  EXPECT_CALL(detector_.barcode_detector(), Detect)
      .WillOnce([](const SkBitmap&,
                   shape_detection::mojom::BarcodeDetection::DetectCallback
                       callback) { std::move(callback).Run({}); });

  base::RunLoop run_loop;
  detector_.Detect(
      web_contents(), TestOptions(), /*barcode_formats=*/{},
      base::BindLambdaForTesting([&](BarcodeDetectionStatus status,
                                     std::vector<WalletBarcode> results) {
        EXPECT_EQ(status, BarcodeDetectionStatus::kSuccess);
        EXPECT_TRUE(results.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(WalletablePassBarcodeDetectorImplTest, QRCodeFound) {
  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce([](mojom::ImageExtractionOptionsPtr,
                   mojom::ImageExtractor::ExtractImagesCallback callback) {
        std::move(callback).Run(MakeImages({mojom::ImageSourceElement::kImg}));
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
      web_contents(), TestOptions(), /*barcode_formats=*/{},
      base::BindLambdaForTesting(
          [&](BarcodeDetectionStatus status,
              std::vector<WalletBarcode> results) {
            EXPECT_EQ(status, BarcodeDetectionStatus::kSuccess);
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
      .WillOnce([&](mojom::ImageExtractionOptionsPtr,
                    mojom::ImageExtractor::ExtractImagesCallback) {
        detector_.image_extractor().Disconnect();
      });

  base::RunLoop run_loop;
  detector_.Detect(
      web_contents(), TestOptions(), /*barcode_formats=*/{},
      base::BindLambdaForTesting([&](BarcodeDetectionStatus status,
                                     std::vector<WalletBarcode> results) {
        EXPECT_EQ(status, BarcodeDetectionStatus::kDetectorUnavailable);
        EXPECT_TRUE(results.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(WalletablePassBarcodeDetectorImplTest,
       BarcodeDetectorRemoteDisconnected) {
  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce([](mojom::ImageExtractionOptionsPtr,
                   mojom::ImageExtractor::ExtractImagesCallback callback) {
        std::move(callback).Run(MakeImages({mojom::ImageSourceElement::kImg}));
      });

  EXPECT_CALL(detector_.barcode_detector(), Detect)
      .WillOnce([&](const SkBitmap&,
                    shape_detection::mojom::BarcodeDetection::DetectCallback) {
        detector_.barcode_detector().Disconnect();
      });

  base::RunLoop run_loop;
  detector_.Detect(
      web_contents(), TestOptions(), /*barcode_formats=*/{},
      base::BindLambdaForTesting([&](BarcodeDetectionStatus status,
                                     std::vector<WalletBarcode> results) {
        EXPECT_EQ(status, BarcodeDetectionStatus::kDetectorUnavailable);
        EXPECT_TRUE(results.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(WalletablePassBarcodeDetectorImplTest, MultipleBarcodesFound) {
  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce([](mojom::ImageExtractionOptionsPtr,
                   mojom::ImageExtractor::ExtractImagesCallback callback) {
        std::move(callback).Run(MakeImages({mojom::ImageSourceElement::kImg,
                                            mojom::ImageSourceElement::kImg}));
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
      web_contents(), TestOptions(), /*barcode_formats=*/{},
      base::BindLambdaForTesting(
          [&](BarcodeDetectionStatus status,
              std::vector<WalletBarcode> results) {
            EXPECT_EQ(status, BarcodeDetectionStatus::kSuccess);
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

// Tests that each decoded barcode reports the element of the image that it was
// decoded from, even if the detections finish out of order.
TEST_F(WalletablePassBarcodeDetectorImplTest, ReportsSourceElement) {
  EXPECT_CALL(detector_.image_extractor(), ExtractImages)
      .WillOnce([](mojom::ImageExtractionOptionsPtr,
                   mojom::ImageExtractor::ExtractImagesCallback callback) {
        std::move(callback).Run(
            MakeImages({mojom::ImageSourceElement::kImg,
                        mojom::ImageSourceElement::kCanvas}));
      });
  std::vector<shape_detection::mojom::BarcodeDetection::DetectCallback>
      detect_callbacks;
  base::RunLoop detect_run_loop;
  EXPECT_CALL(detector_.barcode_detector(), Detect)
      .Times(2)
      .WillRepeatedly(
          [&](const SkBitmap&,
              shape_detection::mojom::BarcodeDetection::DetectCallback
                  callback) {
            detect_callbacks.push_back(std::move(callback));
            if (detect_callbacks.size() == 2) {
              detect_run_loop.Quit();
            }
          });

  base::test::TestFuture<BarcodeDetectionStatus, std::vector<WalletBarcode>>
      future;
  detector_.Detect(web_contents(), TestOptions(), /*barcode_formats=*/{},
                   future.GetCallback());
  detect_run_loop.Run();
  ASSERT_EQ(detect_callbacks.size(), 2u);
  // Finishes the canvas image's detection first.
  std::vector<shape_detection::mojom::BarcodeDetectionResultPtr>
      canvas_barcodes;
  canvas_barcodes.push_back(CreateBarcodeDetectionResult(
      "canvas_value", shape_detection::mojom::BarcodeFormat::QR_CODE));
  std::move(detect_callbacks[1]).Run(std::move(canvas_barcodes));
  std::vector<shape_detection::mojom::BarcodeDetectionResultPtr> img_barcodes;
  img_barcodes.push_back(CreateBarcodeDetectionResult(
      "img_value", shape_detection::mojom::BarcodeFormat::QR_CODE));
  std::move(detect_callbacks[0]).Run(std::move(img_barcodes));

  EXPECT_EQ(future.Get<0>(), BarcodeDetectionStatus::kSuccess);
  EXPECT_THAT(
      future.Get<1>(),
      UnorderedElementsAre(
          WalletBarcode{.raw_value = "img_value",
                        .format = WalletBarcodeFormat::QR_CODE,
                        .source_element = WalletBarcodeSourceElement::kImg},
          WalletBarcode{
              .raw_value = "canvas_value",
              .format = WalletBarcodeFormat::QR_CODE,
              .source_element = WalletBarcodeSourceElement::kCanvas}));
}

}  // namespace
}  // namespace wallet
