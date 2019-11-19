// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_view.h"

namespace chromecast {

std::unique_ptr<content::BluetoothChooser>
CastWebView::Delegate::RunBluetoothChooser(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
  return nullptr;
}

CastWebView::CastWebView(const CreateParams& create_params)
    : delegate_(create_params.delegate),
      shutdown_delay_(create_params.shutdown_delay) {}

CastWebView::~CastWebView() {
  for (Observer& observer : observer_list_) {
    observer.OnPageDestroyed(this);
  }
}

void CastWebView::ForceClose() {
  shutdown_delay_ = base::TimeDelta();
  ClosePage();
}

void CastWebView::AddObserver(CastWebView::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CastWebView::RemoveObserver(CastWebView::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

CastWebView::CreateParams::CreateParams() = default;
CastWebView::CreateParams::CreateParams(const CreateParams& other) = default;
CastWebView::CreateParams::~CreateParams() = default;

}  // namespace chromecast
