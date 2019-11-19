// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_content_window.h"

namespace chromecast {

CastContentWindow::CastContentWindow(const CreateParams& params)
    : delegate_(params.delegate) {}

CastContentWindow::~CastContentWindow() = default;

CastContentWindow::CreateParams::CreateParams() = default;
CastContentWindow::CreateParams::CreateParams(const CreateParams& other) =
    default;
CastContentWindow::CreateParams::~CreateParams() = default;

void CastContentWindow::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CastContentWindow::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

mojom::MediaControlUi* CastContentWindow::media_controls() {
  return nullptr;
}

}  // namespace chromecast
