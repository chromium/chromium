// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/data_sharing_internals/data_sharing_internals_page_handler_impl.h"

#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"

DataSharingInternalsPageHandlerImpl::DataSharingInternalsPageHandlerImpl(
    mojo::PendingReceiver<data_sharing_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<data_sharing_internals::mojom::Page> page,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      data_sharing_service_(
          data_sharing::DataSharingServiceFactory::GetForProfile(profile)) {
  // TODO(qinmin): adding this class as an observer to |data_sharing_service_|.
}

DataSharingInternalsPageHandlerImpl::~DataSharingInternalsPageHandlerImpl() =
    default;

void DataSharingInternalsPageHandlerImpl::IsEmptyService(
    IsEmptyServiceCallback callback) {
  std::move(callback).Run(data_sharing_service_->IsEmptyService());
}
