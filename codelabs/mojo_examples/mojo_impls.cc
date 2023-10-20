// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "codelabs/mojo_examples/mojo_impls.h"

#include "base/logging.h"

ObjectAImpl::ObjectAImpl() = default;
ObjectAImpl::~ObjectAImpl() = default;

void ObjectAImpl::BindToFrozenTaskRunner(
    mojo::PendingAssociatedReceiver<codelabs::mojom::ObjectA> pending_receiver,
    scoped_refptr<base::SingleThreadTaskRunner> freezable_tq_runner) {
  receiver_.Bind(std::move(pending_receiver), std::move(freezable_tq_runner));
}

void ObjectAImpl::DoA() {
  LOG(INFO) << "DoA IPC is being processed!";
}

ObjectBImpl::ObjectBImpl() = default;
ObjectBImpl::~ObjectBImpl() = default;

void ObjectBImpl::Bind(mojo::PendingAssociatedReceiver<codelabs::mojom::ObjectB>
                           pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void ObjectBImpl::DoB() {
  LOG(INFO) << "DoB IPC is being processed!";
}
