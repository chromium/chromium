// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_TEST_UTILS_H_
#define COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_TEST_UTILS_H_

#include <string>

#include "base/functional/callback.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace storage {

namespace test {

// Reads all data from the given |handle| and returns data as a string.
// This is similar to mojo::BlockingCopyToString() but a bit different. This
// doesn't wait synchronously but keeps posting a task when |handle| returns
// MOJO_RESULT_SHOULD_WAIT. In some tests, waiting for consumer handles
// synchronously doesn't work because producers and consumers live on the same
// sequence.
// TODO(bashi): Make producers and consumers live on different sequences then
// use mojo::BlockingCopyToString().
std::string ReadDataPipeViaRunLoop(mojo::ScopedDataPipeConsumerHandle handle);

}  // namespace test
}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_TEST_UTILS_H_
