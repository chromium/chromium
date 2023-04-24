// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
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
    request->should_render = true;
    request->center_image = center_image;
    request->render_module_style = module_style;
    request->render_locator_style = locator_style;

    mojom::GenerateQRCodeResponsePtr response;
    mojo::Remote<mojom::QRCodeGeneratorService> qr_service =
        LaunchQRCodeGeneratorService();
    base::RunLoop run_loop;
    qr_service->GenerateQRCode(
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

    // The `data_size` should indicate a square + should be consistent with
    // the size of the actual `data`.
    ASSERT_EQ(response->data_size.width(), response->data_size.height());
    ASSERT_EQ(static_cast<int32_t>(response->data.size()),
              response->data_size.width() * response->data_size.height());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
    // Verify image contents through go/chrome-engprod-skia-gold.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            "browser-ui-tests-verify-pixels")) {
      const ::testing::TestInfo* test_info =
          ::testing::UnitTest::GetInstance()->current_test_info();
      ui::test::SkiaGoldPixelDiff pixel_diff;
      pixel_diff.Init(test_info->test_suite_name());
      ASSERT_TRUE(
          pixel_diff.CompareScreenshot(test_info->name(), response->bitmap));
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
