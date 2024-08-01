// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEGMENTATION_INTERNALS_SEGMENTATION_INTERNALS_PAGE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SEGMENTATION_INTERNALS_SEGMENTATION_INTERNALS_PAGE_HANDLER_IMPL_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals.mojom.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/service_proxy.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class SegmentationInternalsPageHandlerImpl
    : public segmentation_internals::mojom::PageHandler,
      public segmentation_platform::ServiceProxy::Observer {
 public:
  SegmentationInternalsPageHandlerImpl(
      mojo::PendingReceiver<segmentation_internals::mojom::PageHandler>
          receiver,
      mojo::PendingRemote<segmentation_internals::mojom::Page> page,
      segmentation_platform::SegmentationPlatformService* segmentation_service);
  ~SegmentationInternalsPageHandlerImpl() override;

  SegmentationInternalsPageHandlerImpl(
      const SegmentationInternalsPageHandlerImpl&) = delete;
  SegmentationInternalsPageHandlerImpl& operator=(
      const SegmentationInternalsPageHandlerImpl&) = delete;

  // segmentation_internals::mojom::PageHandler:
  void GetServiceStatus() override;
  void ExecuteModel(int segment_id) override;
  void OverwriteResult(int segment_id, float result) override;
  void SetSelected(const std::string& segmentation_key,
                   int segment_id) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SegmentationInternalsPageHandlerImplTest,
                           EmptyClientInfo);
  FRIEND_TEST_ALL_PREFIXES(SegmentationInternalsPageHandlerImplTest,
                           ClientInfoNotified);

  // segmentation_platform::ServiceProxy::Observer overrides.
  void OnServiceStatusChanged(bool is_initialized, int status_flag) override;
  void OnClientInfoAvailable(
      const std::vector<segmentation_platform::ServiceProxy::ClientInfo>&
          client_info) override;

  mojo::Receiver<segmentation_internals::mojom::PageHandler> receiver_;
  mojo::Remote<segmentation_internals::mojom::Page> page_;
  raw_ptr<segmentation_platform::ServiceProxy> service_proxy_;

  base::WeakPtrFactory<SegmentationInternalsPageHandlerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEGMENTATION_INTERNALS_SEGMENTATION_INTERNALS_PAGE_HANDLER_IMPL_H_
