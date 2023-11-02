// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_TEST_MOCK_LOGGER_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_TEST_MOCK_LOGGER_H_

#include "components/media_router/common/mojom/logger.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class MockLogger : public media_router::mojom::Logger {
 public:
  MockLogger();
  ~MockLogger() override;

  MOCK_METHOD6(LogInfo,
               void(media_router::mojom::LogCategory,
                    const std::string&,
                    const std::string&,
                    const std::string&,
                    const std::string&,
                    const std::string&));
  MOCK_METHOD6(LogWarning,
               void(media_router::mojom::LogCategory,
                    const std::string&,
                    const std::string&,
                    const std::string&,
                    const std::string&,
                    const std::string&));
  MOCK_METHOD6(LogError,
               void(media_router::mojom::LogCategory,
                    const std::string&,
                    const std::string&,
                    const std::string&,
                    const std::string&,
                    const std::string&));

  MOCK_METHOD1(BindReceiver, void(mojo::PendingReceiver<mojom::Logger>));

 private:
  mojo::Receiver<media_router::mojom::Logger> receiver_{this};
};
}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_TEST_MOCK_LOGGER_H_
