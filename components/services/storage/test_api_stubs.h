// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_TEST_API_STUBS_H_
#define COMPONENTS_SERVICES_STORAGE_TEST_API_STUBS_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace storage {

// Allows test environments to inject an actual implementation of the TestApi
// Mojo interface. This uses a raw message pipe to avoid production code
// depending on any test API interface definitions.
using TestApiBinderForTesting =
    base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle)>;

COMPONENT_EXPORT(STORAGE_SERVICE_TEST_API_STUBS)
void SetTestApiBinderForTesting(TestApiBinderForTesting binder);

COMPONENT_EXPORT(STORAGE_SERVICE_TEST_API_STUBS)
const TestApiBinderForTesting& GetTestApiBinderForTesting();

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_TEST_API_STUBS_H_
