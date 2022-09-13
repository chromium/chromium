// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/content/common/mojom/distiller_javascript_service.mojom.h"
#include "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace dom_distiller {

class DomDistillerService;

// This is the receiving end of "distiller" JavaScript object calls.
class DistillerJavaScriptServiceImpl
    : public mojom::DistillerJavaScriptService {
 public:
  explicit DistillerJavaScriptServiceImpl(
      base::WeakPtr<DomDistillerService> distiller_service_weak_ptr);
  ~DistillerJavaScriptServiceImpl() override;

  // Mojo mojom::DistillerJavaScriptService implementation.

  // Show the Android view containing Reader Mode settings.
  void HandleDistillerOpenSettingsCall() override;

  void HandleStoreThemePref(mojom::Theme theme) override;
  void HandleStoreFontFamilyPref(mojom::FontFamily font_family) override;
  void HandleStoreFontScalingPref(float font_scale) override;

  DistillerJavaScriptServiceImpl(const DistillerJavaScriptServiceImpl&) =
      delete;
  DistillerJavaScriptServiceImpl& operator=(
      const DistillerJavaScriptServiceImpl&) = delete;

 private:
  base::WeakPtr<DomDistillerService> distiller_service_weak_ptr_;
};

// static
void CreateDistillerJavaScriptService(
    base::WeakPtr<DomDistillerService> distiller_service_weak_ptr,
    mojo::PendingReceiver<mojom::DistillerJavaScriptService> receiver);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_
