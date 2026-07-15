// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_app_loading_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"

namespace payments {

PaymentAppLoadingView::PaymentAppLoadingView(
    const SkBitmap* icon,
    const GURL& app_origin,
    const GURL& top_origin,
    views::Button::PressedCallback close_callback) {}

PaymentAppLoadingView::~PaymentAppLoadingView() = default;

BEGIN_METADATA(PaymentAppLoadingView)
END_METADATA

}  // namespace payments
