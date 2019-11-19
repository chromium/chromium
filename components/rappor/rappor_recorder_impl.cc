// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/rappor_recorder_impl.h"

#include <memory>

#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace rappor {

RapporRecorderImpl::RapporRecorderImpl(RapporServiceImpl* rappor_service)
    : rappor_service_(rappor_service) {}

RapporRecorderImpl::~RapporRecorderImpl() = default;

// static
void RapporRecorderImpl::Create(
    RapporServiceImpl* rappor_service,
    mojo::PendingReceiver<mojom::RapporRecorder> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<RapporRecorderImpl>(rappor_service),
      std::move(receiver));
}

void RapporRecorderImpl::RecordRappor(const std::string& metric,
                                      const std::string& sample) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SampleString(rappor_service_, metric, ETLD_PLUS_ONE_RAPPOR_TYPE, sample);
}

void RapporRecorderImpl::RecordRapporURL(const std::string& metric,
                                         const GURL& sample) {
  DCHECK(thread_checker_.CalledOnValidThread());
  SampleDomainAndRegistryFromGURL(rappor_service_, metric, sample);
}

}  // namespace rappor
