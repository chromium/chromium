// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_RESPONSE_CALLBACK_WRAPPER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_RESPONSE_CALLBACK_WRAPPER_IMPL_H_

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/babelorca/response_callback_wrapper.h"

namespace ash::babelorca {

template <typename ResponseType>
class ResponseCallbackWrapperImpl : public ResponseCallbackWrapper {
 public:
  using ResponseExpectedCallback = base::OnceCallback<void(
      base::expected<ResponseType, TachyonRequestError>)>;

  explicit ResponseCallbackWrapperImpl(ResponseExpectedCallback callback)
      : callback_(std::move(callback)) {}

  ResponseCallbackWrapperImpl(const ResponseCallbackWrapperImpl&) = delete;
  ResponseCallbackWrapperImpl& operator=(const ResponseCallbackWrapperImpl&) =
      delete;

  ~ResponseCallbackWrapperImpl() override = default;

  void Run(base::expected<std::string, TachyonRequestError> response) override {
    CHECK(callback_);
    if (!response.has_value()) {
      std::move(callback_).Run(base::unexpected(response.error()));
      return;
    }
    auto posted_cb = base::BindPostTaskToCurrentDefault(std::move(callback_));
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(ParseAndReply, std::move(response.value()),
                                  std::move(posted_cb)));
  }

 private:
  static void ParseAndReply(std::string response_string,
                            ResponseExpectedCallback callback) {
    ResponseType response_proto;
    if (!response_proto.ParseFromString(response_string)) {
      std::move(callback).Run(
          base::unexpected(TachyonRequestError::kInternalError));
      return;
    }
    std::move(callback).Run(std::move(response_proto));
  }

  ResponseExpectedCallback callback_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_RESPONSE_CALLBACK_WRAPPER_IMPL_H_
