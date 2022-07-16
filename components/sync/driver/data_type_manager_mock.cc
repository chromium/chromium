// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/data_type_manager_mock.h"

#include "components/sync/driver/configure_context.h"

namespace syncer {

DataTypeManagerMock::DataTypeManagerMock() : result_(OK, ModelTypeSet()) {}

DataTypeManagerMock::~DataTypeManagerMock() = default;

}  // namespace syncer
