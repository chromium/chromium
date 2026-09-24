// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_TRACING_INSTANCE_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_TRACING_INSTANCE_H_

#include <string>
#include <vector>

#include "chromeos/ash/experiences/arc/mojom/tracing.mojom.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace arc {

class FakeTracingInstance : public mojom::TracingInstance {
 public:
  FakeTracingInstance();
  ~FakeTracingInstance() override;

  FakeTracingInstance(const FakeTracingInstance&) = delete;
  FakeTracingInstance& operator=(const FakeTracingInstance&) = delete;

  // mojom::TracingInstance:
  void QueryAvailableCategories(
      QueryAvailableCategoriesCallback callback) override;
  void StartTracing(const std::vector<std::string>& categories,
                    mojo::PlatformHandle socket,
                    StartTracingCallback callback) override;
  void StopTracing(StopTracingCallback callback) override;

  int start_count() const { return start_count_; }
  int stop_count() const { return stop_count_; }
  const mojo::PlatformHandle& socket() const { return socket_; }
  const std::vector<std::string>& start_categories() {
    return start_categories_;
  }

 private:
  int start_count_ = 0;
  std::vector<std::string> start_categories_;
  mojo::PlatformHandle socket_;
  int stop_count_ = 0;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_TRACING_INSTANCE_H_
