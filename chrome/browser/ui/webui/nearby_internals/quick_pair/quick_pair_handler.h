// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_QUICK_PAIR_QUICK_PAIR_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_QUICK_PAIR_QUICK_PAIR_HANDLER_H_

#include <memory>
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace quick_pair {
class FastPairNotificationController;
class FastPairImageDecoder;
}  // namespace quick_pair
}  // namespace ash

// WebUIMessageHandler for the Quick Pair debug page at
// chrome://nearby-internals
class QuickPairHandler : public content::WebUIMessageHandler {
 public:
  QuickPairHandler();
  QuickPairHandler(const QuickPairHandler&) = delete;
  QuickPairHandler& operator=(const QuickPairHandler&) = delete;
  ~QuickPairHandler() override;

  // content::WebUIMessageHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Fast Pair UI Triggers.
  void NotifyFastPairError(const base::Value::List& args);
  void NotifyFastPairDiscovery(const base::Value::List& args);
  void NotifyFastPairPairing(const base::Value::List& args);
  void NotifyFastPairApplicationAvailable(const base::Value::List& args);
  void NotifyFastPairApplicationInstalled(const base::Value::List& args);
  void NotifyFastPairAssociateAccountKey(const base::Value::List& args);

  void OnImageDecodedFastPairError(gfx::Image image);
  void OnImageDecodedFastPairDiscovery(gfx::Image image);
  void OnImageDecodedFastPairPairing(gfx::Image image);
  void OnImageDecodedFastPairApplicationAvailable(gfx::Image image);
  void OnImageDecodedFastPairApplicationInstalled(gfx::Image image);
  void OnImageDecodedFastPairAssociateAccountKey(gfx::Image image);

  std::unique_ptr<ash::quick_pair::FastPairNotificationController>
      fast_pair_notification_controller_;
  std::unique_ptr<ash::quick_pair::FastPairImageDecoder> image_decoder_;

  base::WeakPtrFactory<QuickPairHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_INTERNALS_QUICK_PAIR_QUICK_PAIR_HANDLER_H_
