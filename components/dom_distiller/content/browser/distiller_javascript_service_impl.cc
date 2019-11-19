// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distiller_javascript_service_impl.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace dom_distiller {

DistillerJavaScriptServiceImpl::DistillerJavaScriptServiceImpl(
    DistillerUIHandle* distiller_ui_handle)
    : distiller_ui_handle_(distiller_ui_handle) {}

DistillerJavaScriptServiceImpl::~DistillerJavaScriptServiceImpl() {}

void DistillerJavaScriptServiceImpl::HandleDistillerOpenSettingsCall() {
  if (!distiller_ui_handle_) {
    return;
  }

  distiller_ui_handle_->OpenSettings();
}

void CreateDistillerJavaScriptService(
    DistillerUIHandle* distiller_ui_handle,
    mojo::PendingReceiver<mojom::DistillerJavaScriptService> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DistillerJavaScriptServiceImpl>(distiller_ui_handle),
      std::move(receiver));
}

}  // namespace dom_distiller
