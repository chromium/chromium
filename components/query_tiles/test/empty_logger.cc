// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/test/empty_logger.h"

#include "base/values.h"

namespace query_tiles {
namespace test {

void EmptyLogger::AddObserver(Observer* observer) {}

void EmptyLogger::RemoveObserver(Observer* observer) {}

base::Value EmptyLogger::GetServiceStatus() {
  return base::Value();
}

base::Value EmptyLogger::GetTileData() {
  return base::Value();
}

}  // namespace test
}  // namespace query_tiles
