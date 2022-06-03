// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distiller_javascript_service_impl.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace dom_distiller {

DistillerJavaScriptServiceImpl::DistillerJavaScriptServiceImpl(
    DistillerUIHandle* distiller_ui_handle,
    DistilledPagePrefs* distilled_page_prefs)
    : distiller_ui_handle_(distiller_ui_handle),
      distilled_page_prefs_(distilled_page_prefs) {}

DistillerJavaScriptServiceImpl::~DistillerJavaScriptServiceImpl() = default;

void DistillerJavaScriptServiceImpl::HandleDistillerOpenSettingsCall() {
  if (!distiller_ui_handle_) {
    return;
  }

  distiller_ui_handle_->OpenSettings();
}

void DistillerJavaScriptServiceImpl::HandleStoreThemePref(mojom::Theme theme) {
  distilled_page_prefs_->SetTheme(theme);
}

void DistillerJavaScriptServiceImpl::HandleStoreFontFamilyPref(
    mojom::FontFamily font_family) {
  distilled_page_prefs_->SetFontFamily(font_family);
}

void DistillerJavaScriptServiceImpl::HandleStoreFontScalingPref(
    float font_scale) {
  distilled_page_prefs_->SetFontScaling(font_scale);
}

void CreateDistillerJavaScriptService(
    DistillerUIHandle* distiller_ui_handle,
    DistilledPagePrefs* distilled_page_prefs,
    mojo::PendingReceiver<mojom::DistillerJavaScriptService> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<DistillerJavaScriptServiceImpl>(
                                  distiller_ui_handle, distilled_page_prefs),
                              std::move(receiver));
}

}  // namespace dom_distiller
