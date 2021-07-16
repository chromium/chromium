// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view.h"

namespace chromecast {

CastWebView::CreateParams::CreateParams() = default;

CastWebView::CreateParams::CreateParams(const CreateParams& other) = default;

CastWebView::CreateParams::~CreateParams() = default;

void CastWebView::BindReceivers(
    mojo::PendingReceiver<mojom::CastWebContents> web_contents_receiver,
    mojo::PendingReceiver<mojom::CastContentWindow> window_receiver) {
  cast_web_contents()->BindReceiver(std::move(web_contents_receiver));
  window()->BindReceiver(std::move(window_receiver));
}

}  // namespace chromecast
