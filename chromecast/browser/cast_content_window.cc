// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window.h"

namespace chromecast {

CastContentWindow::CastContentWindow(base::WeakPtr<Delegate> delegate,
                                     mojom::CastWebViewParamsPtr params)
    : delegate_(delegate), params_(std::move(params)) {}

CastContentWindow::~CastContentWindow() = default;

void CastContentWindow::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CastContentWindow::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void CastContentWindow::BindReceiver(
    mojo::PendingReceiver<mojom::CastContentWindow> receiver) {
  receiver_.Bind(std::move(receiver));
}

mojom::MediaControlUi* CastContentWindow::media_controls() {
  return nullptr;
}

}  // namespace chromecast
