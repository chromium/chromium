// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/content/browser/distiller_javascript_service_impl.h"

#include "components/web_distiller/core/web_distiller_service.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace web_distiller {

DistillerJavaScriptServiceImpl::DistillerJavaScriptServiceImpl(
    base::WeakPtr<WebDistillerService> distiller_service_weak_ptr)
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

  distiller_service_weak_ptr_.get()->GetDistilledPagePrefs()->SetUserPrefTheme(
      theme);
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

  distiller_service_weak_ptr_.get()
      ->GetDistilledPagePrefs()
      ->SetUserPrefFontScaling(font_scale);
}

void CreateDistillerJavaScriptService(
    base::WeakPtr<WebDistillerService> distiller_service_weak_ptr,
    mojo::PendingReceiver<mojom::DistillerJavaScriptService> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<DistillerJavaScriptServiceImpl>(
                                  distiller_service_weak_ptr),
                              std::move(receiver));
}

}  // namespace web_distiller
