// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_app_install_util.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
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

  GURL service_worker_javascript_file_url =
      test_server.GetURL(hostname, service_worker_file_path);

  std::string payment_method_identifier =
      url::Origin::Create(service_worker_javascript_file_url).Serialize();

  return InstallPaymentAppForPaymentMethodIdentifier(
             web_contents, service_worker_javascript_file_url,
             payment_method_identifier, icon_install)
             ? payment_method_identifier
             : std::string();
}

// static
bool PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
    content::WebContents& web_contents,
    const GURL& service_worker_javascript_file_url,
    const std::string& payment_method_identifier,
    IconInstall icon_install) {
  CHECK(service_worker_javascript_file_url.is_valid());
  CHECK(!payment_method_identifier.empty());
  CHECK_NE('/', payment_method_identifier.at(
                    payment_method_identifier.length() - 1));

  GURL service_worker_scope =
      service_worker_javascript_file_url.GetWithoutFilename();

  SkBitmap app_icon;
  if (icon_install != IconInstall::kWithoutIcon) {
    const int kBitmapDimension =
        icon_install == IconInstall::kWithLargeIcon ? 128 : 16;
    app_icon.allocN32Pixels(kBitmapDimension, kBitmapDimension);
    app_icon.eraseColor(SK_ColorRED);
  }

  base::RunLoop run_loop;
  bool success = false;
  content::PaymentAppProvider::GetOrCreateForWebContents(&web_contents)
      ->InstallPaymentAppForTesting(
          app_icon, service_worker_javascript_file_url, service_worker_scope,
          payment_method_identifier,
          base::BindOnce(&OnInstallPaymentApp, run_loop.QuitClosure(),
                         &success));
  run_loop.Run();

  return success;
}

}  // namespace payments
