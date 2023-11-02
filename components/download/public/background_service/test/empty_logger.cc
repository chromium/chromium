// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/test/empty_logger.h"

#include "base/values.h"

namespace download {
namespace test {

void EmptyLogger::AddObserver(Observer* observer) {}

void EmptyLogger::RemoveObserver(Observer* observer) {}

base::Value::Dict EmptyLogger::GetServiceStatus() {
  return base::Value::Dict();
}

base::Value::List EmptyLogger::GetServiceDownloads() {
  return base::Value::List();
}

}  // namespace test
}  // namespace download
