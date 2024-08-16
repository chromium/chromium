// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_PAGE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_PAGE_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/data_sharing_internals/data_sharing_internals.mojom.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/protocol/group_data.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class DataSharingInternalsPageHandlerImpl
    : public data_sharing_internals::mojom::PageHandler,
      public data_sharing::DataSharingService::Observer {
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

 private:
  void OnGetAllGroupsDone(
      GetAllGroupsCallback callback,
      const data_sharing::DataSharingService::GroupsDataSetOrFailureOutcome&
          group_result);

  mojo::Receiver<data_sharing_internals::mojom::PageHandler> receiver_;
  mojo::Remote<data_sharing_internals::mojom::Page> page_;

  raw_ptr<data_sharing::DataSharingService> data_sharing_service_;

  base::WeakPtrFactory<DataSharingInternalsPageHandlerImpl> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DATA_SHARING_INTERNALS_DATA_SHARING_INTERNALS_PAGE_HANDLER_IMPL_H_
