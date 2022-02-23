// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/chromium_api_delegate.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "build/buildflag.h"
#include "chromeos/assistant/internal/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {
namespace libassistant {

ChromiumApiDelegate::ChromiumApiDelegate(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory)
    : http_connection_factory_(std::move(pending_url_loader_factory)) {}

ChromiumApiDelegate::~ChromiumApiDelegate() = default;

#if !BUILDFLAG(IS_PREBUILT_LIBASSISTANT)
assistant_client::HttpConnectionFactory*
ChromiumApiDelegate::GetHttpConnectionFactory() {
  return &http_connection_factory_;
}
#endif  // !BUILDFLAG(IS_PREBUILT_LIBASSISTANT)

}  // namespace libassistant
}  // namespace chromeos
