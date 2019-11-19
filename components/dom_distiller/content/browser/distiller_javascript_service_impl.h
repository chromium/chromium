// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_

#include "base/macros.h"
#include "components/dom_distiller/content/common/mojom/distiller_javascript_service.mojom.h"
#include "components/dom_distiller/core/distiller_ui_handle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace dom_distiller {

// This is the receiving end of "distiller" JavaScript object calls.
class DistillerJavaScriptServiceImpl
    : public mojom::DistillerJavaScriptService {
 public:
  DistillerJavaScriptServiceImpl(DistillerUIHandle* distiller_ui_handle);
  ~DistillerJavaScriptServiceImpl() override;

  // Mojo mojom::DistillerJavaScriptService implementation.

  // Show the Android view containing Reader Mode settings.
  void HandleDistillerOpenSettingsCall() override;

 private:
  DistillerUIHandle* distiller_ui_handle_;

  DISALLOW_COPY_AND_ASSIGN(DistillerJavaScriptServiceImpl);
};

// static
void CreateDistillerJavaScriptService(
    DistillerUIHandle* distiller_ui_handle,
    mojo::PendingReceiver<mojom::DistillerJavaScriptService> receiver);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLER_JAVASCRIPT_SERVICE_IMPL_H_
