// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/test_web_contents_modal_dialog_host.h"

#include "ui/gfx/geometry/point.h"

namespace web_modal {

TestWebContentsModalDialogHost::TestWebContentsModalDialogHost(
    gfx::NativeView host_view)
    : host_view_(host_view) {}

TestWebContentsModalDialogHost::~TestWebContentsModalDialogHost() = default;

gfx::Size TestWebContentsModalDialogHost::GetMaximumDialogSize() {
  return max_dialog_size_;
}

gfx::NativeView TestWebContentsModalDialogHost::GetHostView() const {
  return host_view_;
}

gfx::Point TestWebContentsModalDialogHost::GetDialogPosition(
    const gfx::Size& size) {
  return gfx::Point();
}

void TestWebContentsModalDialogHost::AddObserver(
    ModalDialogHostObserver* observer) {}

void TestWebContentsModalDialogHost::RemoveObserver(
    ModalDialogHostObserver* observer) {}

}  // namespace web_modal
