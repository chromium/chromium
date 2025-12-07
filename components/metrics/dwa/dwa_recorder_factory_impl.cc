// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_recorder_factory_impl.h"

#include "components/metrics/dwa/dwa_recorder_interface.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace metrics::dwa {

DwaRecorderFactoryImpl::DwaRecorderFactoryImpl(DwaRecorder* dwa_recorder)
    : dwa_recorder_(dwa_recorder) {}

DwaRecorderFactoryImpl::~DwaRecorderFactoryImpl() = default;

// static
void DwaRecorderFactoryImpl::Create(
    DwaRecorder* dwa_recorder,
    mojo::PendingReceiver<metrics::dwa::mojom::DwaRecorderFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DwaRecorderFactoryImpl>(dwa_recorder),
      std::move(receiver));
}

void DwaRecorderFactoryImpl::CreateDwaRecorder(
    mojo::PendingReceiver<metrics::dwa::mojom::DwaRecorderInterface> receiver,
    mojo::PendingRemote<metrics::dwa::mojom::DwaRecorderClientInterface>
        client_remote) {
  DwaRecorderInterface::Create(dwa_recorder_, std::move(receiver));
}

}  // namespace metrics::dwa
