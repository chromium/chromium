// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_HANDLER_WINDOW_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_HANDLER_WINDOW_ANDROID_H_

#include <optional>

#include "base/functional/callback_forward.h"

class GURL;

namespace payments::android {

// Opens a Payment Handler bottom-sheet window on Android for the currently
// showing PaymentRequest. `preferred_height` is the height in CSS pixels that
// the payment app requested for its web content via
// PaymentRequestEvent.openWindow(url, {preferredSize}) (content has already
// validated that it is positive), or std::nullopt if it did not request one,
// in which case a default height matching desktop is used. Invokes
// `callback` once the main frame's first navigation commits, or with
// `(false, 0, 0)` on any failure.
//
// This is the main path of ContentBrowserClient::ShowPaymentHandlerWindow(), so
// `callback` is ServiceWorkerVersion::DidShowPaymentHandlerWindow(), as on
// desktop. Unlike the fallback path (ServiceTabLauncher), on success content
// also sets payment_handler_connected_ and starts the payment handler timeout,
// which suppresses the service worker's idle timeout while the window is open
// (ServiceWorkerSuppressTimeoutWhenPaymentWindowOpen) until
// OnPaymentHandlerDisconnect() or the timeout.
void ShowPaymentHandlerWindow(
    const GURL& url,
    std::optional<int> preferred_height,
    base::OnceCallback<void(bool /* success */,
                            int /* render_process_id */,
                            int /* render_frame_id */)> callback);

}  // namespace payments::android

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_HANDLER_WINDOW_ANDROID_H_
