// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_page_handler_impl.h"

#include "base/time/time.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/features.h"
#include "components/history_clusters/core/history_clusters_service.h"

HistoryClustersInternalsPageHandlerImpl::
    HistoryClustersInternalsPageHandlerImpl(
        mojo::PendingRemote<history_clusters_internals::mojom::Page> page,
        history_clusters::HistoryClustersService* history_clusters_service)
    : page_(std::move(page)),
      history_clusters_service_(history_clusters_service) {
  if (!history_clusters::GetConfig().history_clusters_internals_page) {
    page_->OnLogMessageAdded(
        "History clusters internals page feature is turned off.");
    return;
  }
  if (!history_clusters_service_) {
    page_->OnLogMessageAdded(
        "History clusters service not found for the profile.");
    return;
  }
  history_clusters_service_->AddObserver(this);
}

HistoryClustersInternalsPageHandlerImpl::
    ~HistoryClustersInternalsPageHandlerImpl() {
  if (history_clusters_service_)
    history_clusters_service_->RemoveObserver(this);
}

void HistoryClustersInternalsPageHandlerImpl::OnDebugMessage(
    const std::string& message) {
  page_->OnLogMessageAdded(message);
}
