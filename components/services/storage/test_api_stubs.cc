// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/test_api_stubs.h"

#include "base/callback.h"
#include "base/no_destructor.h"

namespace storage {

namespace {

TestApiBinderForTesting& GetTestApiBinder() {
  static base::NoDestructor<TestApiBinderForTesting> binder;
  return *binder;
}

}  // namespace

void SetTestApiBinderForTesting(TestApiBinderForTesting binder) {
  GetTestApiBinder() = std::move(binder);
}

const TestApiBinderForTesting& GetTestApiBinderForTesting() {
  return GetTestApiBinder();
}

}  // namespace storage
