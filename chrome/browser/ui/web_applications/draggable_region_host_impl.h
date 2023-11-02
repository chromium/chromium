// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_DRAGGABLE_REGION_HOST_IMPL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_DRAGGABLE_REGION_HOST_IMPL_H_

#include "chrome/common/draggable_regions.mojom.h"
#include "content/public/browser/document_service.h"

namespace content {
class RenderFrameHost;
}

class DraggableRegionsHostImpl
    : public content::DocumentService<chrome::mojom::DraggableRegions> {
 public:
  DraggableRegionsHostImpl(const DraggableRegionsHostImpl&) = delete;
  DraggableRegionsHostImpl& operator=(const DraggableRegionsHostImpl&) = delete;
  ~DraggableRegionsHostImpl() override;

  // We only want to create this object when the Browser* associated with the
  // WebContents is a web app and when the RFH is the main frame.
  static void CreateIfAllowed(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<chrome::mojom::DraggableRegions> receiver);

  // chrome::mojom::DraggableRegions
  void UpdateDraggableRegions(
      std::vector<chrome::mojom::DraggableRegionPtr> draggable_region) override;

 private:
  DraggableRegionsHostImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<chrome::mojom::DraggableRegions> receiver);
};

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_DRAGGABLE_REGION_HOST_IMPL_H_
