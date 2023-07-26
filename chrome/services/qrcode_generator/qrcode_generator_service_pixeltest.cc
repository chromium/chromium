// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/base/test/skia_gold_pixel_diff.h"

namespace qrcode_generator {
namespace {

class QrCodeGeneratorServicePixelTest : public PlatformBrowserTest {
 public:
  QrCodeGeneratorServicePixelTest() = default;

  void TestGolden(const std::string& data,
                  const mojom::CenterImage& center_image,
                  const mojom::ModuleStyle& module_style,
                  const mojom::LocatorStyle& locator_style) {
    mojom::GenerateQRCodeRequestPtr request =
        mojom::GenerateQRCodeRequest::New();
    request->data = data;
    request->center_image = center_image;
    request->render_module_style = module_style;
    request->render_locator_style = locator_style;

    base::HistogramTester histograms;
    mojom::GenerateQRCodeResponsePtr response;
    base::RunLoop run_loop;
    QRImageGenerator generator;
    generator.GenerateQRCode(
        std::move(request),
        base::BindLambdaForTesting([&](mojom::GenerateQRCodeResponsePtr r) {
          response = std::move(r);
          run_loop.Quit();
        }));
    run_loop.Run();

    // Verify that we got a successful response.
    ASSERT_TRUE(response);
    ASSERT_EQ(response->error_code, mojom::QRCodeGeneratorError::NONE);

    // Version 1 of QR codes has 21x21 modules/tiles/pixels.  Verify that the
    // returned QR image has a size that is at least 21x21.
    ASSERT_GE(response->data_size.width(), 21);

    // The QR code should be a square.
    ASSERT_EQ(response->data_size.width(), response->data_size.height());
    ASSERT_EQ(response->bitmap.width(), response->bitmap.height());

    // The bitmap size should be a multiple of the QR size.
    ASSERT_EQ(response->bitmap.width() % response->data_size.width(), 0);
    ASSERT_EQ(response->bitmap.height() % response->data_size.height(), 0);

    // Verify that the expected UMA metrics got logged.
    // TODO(1246137): Cover BytesToQrPixels and QrPixelsToQrImage as well.
    histograms.ExpectTotalCount("Sharing.QRCodeGeneration.Duration", 1);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
    // Verify image contents through go/chrome-engprod-skia-gold.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kVerifyPixels)) {
      const ::testing::TestInfo* test_info =
          ::testing::UnitTest::GetInstance()->current_test_info();
      ui::test::SkiaGoldPixelDiff* pixel_diff =
          ui::test::SkiaGoldPixelDiff::GetSession();
      ASSERT_TRUE(pixel_diff);
      ASSERT_TRUE(pixel_diff->CompareScreenshot(
          ui::test::SkiaGoldPixelDiff::GetGoldenImageName(
              test_info, ui::test::SkiaGoldPixelDiff::GetPlatform()),
          response->bitmap));
    }
#endif
  }
};

IN_PROC_BROWSER_TEST_F(QrCodeGeneratorServicePixelTest,
                       DinoWithRoundQrPixelsAndLocators) {
  TestGolden("https://example.com", mojom::CenterImage::CHROME_DINO,
             mojom::ModuleStyle::CIRCLES, mojom::LocatorStyle::ROUNDED);
}

IN_PROC_BROWSER_TEST_F(QrCodeGeneratorServicePixelTest,
                       PassKeyWithSquareQrPixelsAndLocators) {
  TestGolden("https://example.com", mojom::CenterImage::PASSKEY_ICON,
             mojom::ModuleStyle::DEFAULT_SQUARES,
             mojom::LocatorStyle::DEFAULT_SQUARE);
}

}  // namespace
}  // namespace qrcode_generator
