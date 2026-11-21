// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/content/browser/distiller_javascript_service_impl.h"

#include <utility>

#include "base/memory/self_deleting.h"
#include "components/web_distiller/core/distilled_page_prefs.h"
#include "components/web_distiller/core/web_distiller_service.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"

namespace web_distiller {

DistillerJavaScriptServiceImpl::DistillerJavaScriptServiceImpl(
    content::RenderFrameHost& render_frame_host,
    base::WeakPtr<WebDistillerService> distiller_service_weak_ptr,
    mojo::PendingReceiver<mojom::DistillerJavaScriptService> receiver,
    base::SelfDeletingPassKey key)
    : content::DocumentService<mojom::DistillerJavaScriptService>(
          render_frame_host,
          std::move(receiver),
          key),
      distiller_service_weak_ptr_(std::move(distiller_service_weak_ptr)) {}

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
  if (!IsValidFontScale(font_scale)) {
    ReportBadMessageAndDeleteThis("Invalid font scale");
    return;
  }
  if (!distiller_service_weak_ptr_) {
    return;
  }

  distiller_service_weak_ptr_.get()
      ->GetDistilledPagePrefs()
      ->SetUserPrefFontScaling(font_scale);
}

void CreateDistillerJavaScriptService(
    content::RenderFrameHost& render_frame_host,
    base::WeakPtr<WebDistillerService> distiller_service_weak_ptr,
    mojo::PendingReceiver<mojom::DistillerJavaScriptService> receiver) {
  base::MakeSelfDeleting<DistillerJavaScriptServiceImpl>(
      render_frame_host, std::move(distiller_service_weak_ptr),
      std::move(receiver));
}

}  // namespace web_distiller
