// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_EMPTY_LOGGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_EMPTY_LOGGER_H_

#include "components/download/public/background_service/logger.h"

namespace download {
namespace test {

// A Logger that does nothing.
class EmptyLogger : public Logger {
 public:
  EmptyLogger() = default;

  EmptyLogger(const EmptyLogger&) = delete;
  EmptyLogger& operator=(const EmptyLogger&) = delete;

  ~EmptyLogger() override = default;

  // Logger implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::Value::Dict GetServiceStatus() override;
  base::Value::List GetServiceDownloads() override;
};

}  // namespace test
}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_TEST_EMPTY_LOGGER_H_
