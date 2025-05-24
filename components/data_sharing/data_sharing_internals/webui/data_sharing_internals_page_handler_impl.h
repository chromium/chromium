// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_DATA_SHARING_INTERNALS_WEBUI_DATA_SHARING_INTERNALS_PAGE_HANDLER_IMPL_H_
#define COMPONENTS_DATA_SHARING_DATA_SHARING_INTERNALS_WEBUI_DATA_SHARING_INTERNALS_PAGE_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/data_sharing/data_sharing_internals/webui/data_sharing_internals.mojom.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/logger.h"
#include "components/data_sharing/public/protocol/group_data.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class DataSharingInternalsPageHandlerImpl
    : public data_sharing_internals::mojom::PageHandler,
      public data_sharing::DataSharingService::Observer,
      public data_sharing::Logger::Observer {
 public:
  DataSharingInternalsPageHandlerImpl(
      mojo::PendingReceiver<data_sharing_internals::mojom::PageHandler>
          receiver,
      mojo::PendingRemote<data_sharing_internals::mojom::Page> page,
      data_sharing::DataSharingService* data_sharing_service);
  ~DataSharingInternalsPageHandlerImpl() override;

  DataSharingInternalsPageHandlerImpl(
      const DataSharingInternalsPageHandlerImpl&) = delete;
  DataSharingInternalsPageHandlerImpl& operator=(
      const DataSharingInternalsPageHandlerImpl&) = delete;

  // data_sharing_internals::mojom::PageHandler:
  void IsEmptyService(IsEmptyServiceCallback callback) override;
  void GetAllGroups(GetAllGroupsCallback callback) override;

  // data_sharing::Logger::Observer implementation.
  void OnNewLog(const data_sharing::Logger::Entry& entry) override;

 private:
  mojo::Receiver<data_sharing_internals::mojom::PageHandler> receiver_;
  mojo::Remote<data_sharing_internals::mojom::Page> page_;

  raw_ptr<data_sharing::DataSharingService> data_sharing_service_;

  base::WeakPtrFactory<DataSharingInternalsPageHandlerImpl> weak_ptr_factory_{
      this};
};

#endif  // COMPONENTS_DATA_SHARING_DATA_SHARING_INTERNALS_WEBUI_DATA_SHARING_INTERNALS_PAGE_HANDLER_IMPL_H_
