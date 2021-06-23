// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_

#include "components/dom_distiller/content/common/mojom/distiller_javascript_service.mojom.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller_ui_handle.h"
#include "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace dom_distiller {

class DistilledPagePrefs;

// This is the receiving end of "distiller" JavaScript object calls.
class DistillerJavaScriptServiceImpl
    : public mojom::DistillerJavaScriptService {
 public:
  DistillerJavaScriptServiceImpl(DistillerUIHandle* distiller_ui_handle,
                                 DistilledPagePrefs* distilled_page_prefs);
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
  DistillerUIHandle* distiller_ui_handle_;
  DistilledPagePrefs* distilled_page_prefs_;
};

// static
void CreateDistillerJavaScriptService(
    DistillerUIHandle* distiller_ui_handle,
    DistilledPagePrefs* distilled_page_prefs,
    mojo::PendingReceiver<mojom::DistillerJavaScriptService> receiver);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_
