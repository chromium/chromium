// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/fake_gcm_client_factory.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/gcm_driver/gcm_client.h"

namespace gcm {

FakeGCMClientFactory::FakeGCMClientFactory(
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& io_thread)
    : ui_thread_(ui_thread),
      io_thread_(io_thread) {
}

FakeGCMClientFactory::~FakeGCMClientFactory() {
}

std::unique_ptr<GCMClient> FakeGCMClientFactory::BuildInstance() {
  return std::unique_ptr<GCMClient>(new FakeGCMClient(ui_thread_, io_thread_));
}

}  // namespace gcm
