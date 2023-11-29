// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_client_impl.h"

#include "ash/picker/picker_controller.h"
#include "chrome/browser/ui/ash/ash_web_view_impl.h"

PickerClientImpl::PickerClientImpl(ash::PickerController* controller)
    : controller_(controller) {
  controller_->SetClient(this);
}

PickerClientImpl::~PickerClientImpl() {
  controller_->SetClient(nullptr);
}

std::unique_ptr<ash::AshWebView> PickerClientImpl::CreateWebView(
    const ash::AshWebView::InitParams& params) {
  return std::make_unique<AshWebViewImpl>(params);
}
