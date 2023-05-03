// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/testing/mock_objects.h"

namespace allocation_recorder::testing {
namespace crashpad {}  // namespace crashpad

namespace crash_handler {

StreamDataSourceFactoryMock::StreamDataSourceFactoryMock() = default;
StreamDataSourceFactoryMock::~StreamDataSourceFactoryMock() = default;

}  // namespace crash_handler
}  // namespace allocation_recorder::testing
