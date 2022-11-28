// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_app_install_util.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/run_loop.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/supported_delegations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace payments {
namespace {

void OnInstallPaymentApp(base::OnceClosure finished,
                         bool* success_out,
                         bool success) {
  *success_out = success;
  std::move(finished).Run();
}

}  // namespace

// static
std::string PaymentAppInstallUtil::InstallPaymentApp(
    content::WebContents& web_contents,
    net::EmbeddedTestServer& test_server,
    const std::string& hostname,
    const std::string& service_worker_file_path,
    IconInstall icon_install) {
  CHECK(!hostname.empty());
  CHECK(!service_worker_file_path.empty());
  CHECK_EQ('/', service_worker_file_path.at(0));

  GURL sw_js_url = test_server.GetURL(hostname, service_worker_file_path);
  GURL sw_scope = sw_js_url.GetWithoutFilename();

  std::string method = url::Origin::Create(sw_scope).Serialize();
  CHECK_NE('/', method.at(method.length() - 1));

  SkBitmap app_icon;
  if (icon_install == IconInstall::kWithIcon) {
    constexpr int kBitmapDimension = 16;
    app_icon.allocN32Pixels(kBitmapDimension, kBitmapDimension);
    app_icon.eraseColor(SK_ColorRED);
  }

  base::RunLoop run_loop;
  bool success = false;
  content::PaymentAppProvider::GetOrCreateForWebContents(&web_contents)
      ->InstallPaymentAppForTesting(
          app_icon, sw_js_url, sw_scope, method,
          base::BindOnce(&OnInstallPaymentApp, run_loop.QuitClosure(),
                         &success));
  run_loop.Run();

  return success ? method : std::string();
}

}  // namespace payments
