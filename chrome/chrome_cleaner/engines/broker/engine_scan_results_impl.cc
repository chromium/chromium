// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/engine_scan_results_impl.h"

#include <utility>

namespace chrome_cleaner {

EngineScanResultsImpl::EngineScanResultsImpl(
    InterfaceMetadataObserver* metadata_observer)
    : metadata_observer_(metadata_observer) {}

EngineScanResultsImpl::~EngineScanResultsImpl() = default;

void EngineScanResultsImpl::BindToCallbacks(
    mojo::PendingAssociatedRemote<mojom::EngineScanResults>* scan_results,
    FoundUwSCallback found_uws_callback,
    DoneCallback done_callback) {
  receiver_.Bind(scan_results->InitWithNewEndpointAndPassReceiver());
  // There's no need to call set_connection_error_handler on this since it's an
  // associated interface. Any errors will be handled on the main EngineCommands
  // interface.
  found_uws_callback_ = found_uws_callback;
  done_callback_ = std::move(done_callback);
}

void EngineScanResultsImpl::FoundUwS(UwSId pup_id, const PUPData::PUP& pup) {
  // TODO(joenotcharles): Call mojom::ReportBadMessage if these are called out
  // of order.
  DCHECK(found_uws_callback_);
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);

  found_uws_callback_.Run(pup_id, pup);
}

void EngineScanResultsImpl::Done(uint32_t result_code) {
  // TODO(joenotcharles): Call mojom::ReportBadMessage if these are called out
  // of order.
  DCHECK(done_callback_);
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  std::move(done_callback_).Run(result_code);
}

}  // namespace chrome_cleaner
