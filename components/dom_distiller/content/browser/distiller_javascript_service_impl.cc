// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distiller_javascript_service_impl.h"
#include "components/dom_distiller/core/dom_distiller_service.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace dom_distiller {

DistillerJavaScriptServiceImpl::DistillerJavaScriptServiceImpl(
    base::WeakPtr<DomDistillerService> distiller_service_weak_ptr)
    : distiller_service_weak_ptr_(distiller_service_weak_ptr) {}

DistillerJavaScriptServiceImpl::~DistillerJavaScriptServiceImpl() = default;

void DistillerJavaScriptServiceImpl::HandleDistillerOpenSettingsCall() {
  if (!distiller_service_weak_ptr_ ||
      !distiller_service_weak_ptr_->GetDistillerUIHandle()) {
    return;
  }

  distiller_service_weak_ptr_.get()->GetDistillerUIHandle()->OpenSettings();
}

void DistillerJavaScriptServiceImpl::HandleStoreThemePref(mojom::Theme theme) {
  if (!distiller_service_weak_ptr_) {
    return;
  }

  distiller_service_weak_ptr_.get()->GetDistilledPagePrefs()->SetTheme(theme);
}

void DistillerJavaScriptServiceImpl::HandleStoreFontFamilyPref(
    mojom::FontFamily font_family) {
  if (!distiller_service_weak_ptr_) {
    return;
  }

  distiller_service_weak_ptr_.get()->GetDistilledPagePrefs()->SetFontFamily(
      font_family);
}

void DistillerJavaScriptServiceImpl::HandleStoreFontScalingPref(
    float font_scale) {
  if (!distiller_service_weak_ptr_) {
    return;
  }

  distiller_service_weak_ptr_.get()->GetDistilledPagePrefs()->SetFontScaling(
      font_scale);
}

void CreateDistillerJavaScriptService(
    base::WeakPtr<DomDistillerService> distiller_service_weak_ptr,
    mojo::PendingReceiver<mojom::DistillerJavaScriptService> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<DistillerJavaScriptServiceImpl>(
                                  distiller_service_weak_ptr),
                              std::move(receiver));
}

}  // namespace dom_distiller
