// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEGMENTATION_INTERNALS_SEGMENTATION_INTERNALS_PAGE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SEGMENTATION_INTERNALS_SEGMENTATION_INTERNALS_PAGE_HANDLER_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals.mojom.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace segmentation_platform {
class SegmentationPlatformService;
}

class SegmentationInternalsPageHandlerImpl
    : public segmentation_internals::mojom::PageHandler {
 public:
  SegmentationInternalsPageHandlerImpl(
      mojo::PendingReceiver<segmentation_internals::mojom::PageHandler>
          receiver,
      Profile* profile);
  ~SegmentationInternalsPageHandlerImpl() override;

  SegmentationInternalsPageHandlerImpl(
      const SegmentationInternalsPageHandlerImpl&) = delete;
  SegmentationInternalsPageHandlerImpl& operator=(
      const SegmentationInternalsPageHandlerImpl&) = delete;

  // segmentation_internals::mojom::PageHandler:
  void GetSegment(const std::string& key, GetSegmentCallback callback) override;

 private:
  // Called when segment result is retrieved.
  void OnGetSelectedSegmentDone(
      GetSegmentCallback callback,
      const segmentation_platform::SegmentSelectionResult& result);

  mojo::Receiver<segmentation_internals::mojom::PageHandler> receiver_;
  segmentation_platform::SegmentationPlatformService*
      segmentation_platform_service_;

  base::WeakPtrFactory<SegmentationInternalsPageHandlerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEGMENTATION_INTERNALS_SEGMENTATION_INTERNALS_PAGE_HANDLER_IMPL_H_
