// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_PAGE_NOTIFIER_SERVICE_IMPL_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_PAGE_NOTIFIER_SERVICE_IMPL_H_

#include "components/dom_distiller/content/common/mojom/distiller_page_notifier_service.mojom.h"
#include "components/dom_distiller/content/renderer/distiller_js_render_frame_observer.h"
#include "components/dom_distiller/content/renderer/distiller_native_javascript.h"

namespace dom_distiller {

class DistillerJsRenderFrameObserver;

// mojom::DistillerPageNotifierService is responsible for listening to the
// browser for
// messages about if a page is a distiller page. No message is received if the
// page is not a distiller page. This service should be removed from the
// registry once the page is done loading.
class DistillerPageNotifierServiceImpl
    : public mojom::DistillerPageNotifierService {
 public:
  explicit DistillerPageNotifierServiceImpl(
      DistillerJsRenderFrameObserver* observer);
  ~DistillerPageNotifierServiceImpl() override;

  // Implementation of mojo interface DistillerPageNotifierService.
  void NotifyIsDistillerPage() override;

 private:
  DistillerJsRenderFrameObserver* distiller_js_observer_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLER_PAGE_NOTIFIER_SERVICE_IMPL_H_
