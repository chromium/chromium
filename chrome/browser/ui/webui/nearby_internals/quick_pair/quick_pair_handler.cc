// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/quick_pair/quick_pair_handler.h"

#include <memory>
#include <utility>

#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder_impl.h"
#include "ash/quick_pair/ui/fast_pair/fast_pair_notification_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "ui/message_center/message_center.h"

namespace {
// Test device metadata for debug purposes
const char16_t kTestDeviceName[] = u"Pixel Buds";
const char16_t kTestAppName[] = u"JBLTools";
const char16_t kTestEmail[] = u"testemail@gmail.com";
const char kImageUrl[] =
    "https://lh3.googleusercontent.com/"
    "kGH7uF95EhgI0XBRJOGh3l7KvPWsNAFwaxPfksIJloqk-"
    "mh8cZYG9RITPS65UOtUNry9dnyYYMn5dQtFzVdagSE";

}  // namespace

QuickPairHandler::QuickPairHandler()
    : fast_pair_notification_controller_(
          std::make_unique<ash::quick_pair::FastPairNotificationController>(
              message_center::MessageCenter::Get())),
      image_decoder_(
          std::make_unique<ash::quick_pair::FastPairImageDecoderImpl>()) {}

QuickPairHandler::~QuickPairHandler() = default;

void QuickPairHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "notifyFastPairError",
      base::BindRepeating(&QuickPairHandler::NotifyFastPairError,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifyFastPairDiscovery",
      base::BindRepeating(&QuickPairHandler::NotifyFastPairDiscovery,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifyFastPairPairing",
      base::BindRepeating(&QuickPairHandler::NotifyFastPairPairing,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifyFastPairApplicationAvailable",
      base::BindRepeating(&QuickPairHandler::NotifyFastPairApplicationAvailable,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifyFastPairApplicationInstalled",
      base::BindRepeating(&QuickPairHandler::NotifyFastPairApplicationInstalled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "notifyFastPairAssociateAccount",
      base::BindRepeating(&QuickPairHandler::NotifyFastPairAssociateAccountKey,
                          base::Unretained(this)));
}

void QuickPairHandler::OnJavascriptAllowed() {}

void QuickPairHandler::OnJavascriptDisallowed() {}

void QuickPairHandler::NotifyFastPairError(const base::Value::List& args) {
  image_decoder_->DecodeImageFromUrl(
      GURL(kImageUrl),
      /*resize_to_notification_size=*/true,
      base::BindOnce(&QuickPairHandler::OnImageDecodedFastPairError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void QuickPairHandler::OnImageDecodedFastPairError(gfx::Image image) {
  fast_pair_notification_controller_->ShowErrorNotification(
      kTestDeviceName, image, base::DoNothing(), base::DoNothing());
}

void QuickPairHandler::NotifyFastPairDiscovery(const base::Value::List& args) {
  image_decoder_->DecodeImageFromUrl(
      GURL(kImageUrl),
      /*resize_to_notification_size=*/true,
      base::BindOnce(&QuickPairHandler::OnImageDecodedFastPairDiscovery,
                     weak_ptr_factory_.GetWeakPtr()));
}

void QuickPairHandler::OnImageDecodedFastPairDiscovery(gfx::Image image) {
  fast_pair_notification_controller_->ShowGuestDiscoveryNotification(
      kTestDeviceName, image, base::DoNothing(), base::DoNothing(),
      base::DoNothing());
}

void QuickPairHandler::NotifyFastPairPairing(const base::Value::List& args) {
  image_decoder_->DecodeImageFromUrl(
      GURL(kImageUrl),
      /*resize_to_notification_size=*/true,
      base::BindOnce(&QuickPairHandler::OnImageDecodedFastPairPairing,
                     weak_ptr_factory_.GetWeakPtr()));
}

void QuickPairHandler::OnImageDecodedFastPairPairing(gfx::Image image) {
  fast_pair_notification_controller_->ShowPairingNotification(
      kTestDeviceName, image, base::DoNothing());
}

void QuickPairHandler::NotifyFastPairApplicationAvailable(
    const base::Value::List& args) {
  image_decoder_->DecodeImageFromUrl(
      GURL(kImageUrl),
      /*resize_to_notification_size=*/true,
      base::BindOnce(
          &QuickPairHandler::OnImageDecodedFastPairApplicationAvailable,
          weak_ptr_factory_.GetWeakPtr()));
}

void QuickPairHandler::OnImageDecodedFastPairApplicationAvailable(
    gfx::Image image) {
  fast_pair_notification_controller_->ShowApplicationAvailableNotification(
      kTestDeviceName, image, base::DoNothing(), base::DoNothing());
}

void QuickPairHandler::NotifyFastPairApplicationInstalled(
    const base::Value::List& args) {
  image_decoder_->DecodeImageFromUrl(
      GURL(kImageUrl),
      /*resize_to_notification_size=*/true,
      base::BindOnce(
          &QuickPairHandler::OnImageDecodedFastPairApplicationInstalled,
          weak_ptr_factory_.GetWeakPtr()));
}

void QuickPairHandler::OnImageDecodedFastPairApplicationInstalled(
    gfx::Image image) {
  fast_pair_notification_controller_->ShowApplicationInstalledNotification(
      kTestDeviceName, image, kTestAppName, base::DoNothing(),
      base::DoNothing());
}

void QuickPairHandler::NotifyFastPairAssociateAccountKey(
    const base::Value::List& args) {
  image_decoder_->DecodeImageFromUrl(
      GURL(kImageUrl),
      /*resize_to_notification_size=*/true,
      base::BindOnce(
          &QuickPairHandler::OnImageDecodedFastPairAssociateAccountKey,
          weak_ptr_factory_.GetWeakPtr()));
}

void QuickPairHandler::OnImageDecodedFastPairAssociateAccountKey(
    gfx::Image image) {
  fast_pair_notification_controller_->ShowAssociateAccount(
      kTestDeviceName, kTestEmail, image, base::DoNothing(), base::DoNothing(),
      base::DoNothing());
}
