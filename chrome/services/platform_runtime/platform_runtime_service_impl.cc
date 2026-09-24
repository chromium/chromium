// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/platform_runtime/platform_runtime_service_impl.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

namespace platform_runtime {

namespace {

using request_header_integrity::mojom::PlatformRuntimeStatus;

struct HeaderCaptureContext {
  net::HttpRequestHeaders input;
  net::HttpRequestHeaders output;
};

bool GetHeaderCallback(void* ctx,
                       const char* name,
                       char* value_buf,
                       size_t value_buf_size) {
  if (!ctx || !name || !value_buf || value_buf_size == 0) {
    return false;
  }
  auto* context = static_cast<HeaderCaptureContext*>(ctx);
  std::optional<std::string> value = context->input.GetHeader(name);
  if (!value) {
    return false;
  }
  // SAFETY: This implements the library's C-style GetHeaderFunction API, which
  // hands us a raw pointer and a size. They are wrapped in a base::span
  // immediately and all copying below is bounds-checked.
  auto value_span = UNSAFE_BUFFERS(base::span(value_buf, value_buf_size));
  const size_t copy_len = std::min(value->length(), value_buf_size - 1);
  value_span.first(copy_len).copy_from(base::span(*value).first(copy_len));
  value_span[copy_len] = '\0';
  return true;
}

void SetHeaderCallback(void* ctx, const char* name, const char* value) {
  if (!ctx || !name || !value) {
    return;
  }
  auto* context = static_cast<HeaderCaptureContext*>(ctx);
  if (!net::HttpUtil::IsValidHeaderName(name) ||
      !net::HttpUtil::IsValidHeaderValue(value)) {
    return;
  }
  context->output.SetHeader(name, value);
}

}  // namespace

PlatformRuntimeServiceImpl::Runner::Runner(
    const base::FilePath& library_path,
    mojo::PendingReceiver<request_header_integrity::mojom::PlatformRuntime>
        receiver)
    : library_(library_path), receiver_(this, std::move(receiver)) {
  if (library_.is_valid()) {
    process_request_headers_ = reinterpret_cast<ProcessRequestHeadersFunction>(
        library_.GetFunctionPointer("ProcessRequestHeaders"));
    if (!process_request_headers_) {
      library_.reset();
    }
  }
}

PlatformRuntimeServiceImpl::Runner::~Runner() = default;

DISABLE_CFI_DLSYM
void PlatformRuntimeServiceImpl::Runner::ProcessHeaders(
    const net::HttpRequestHeaders& input_headers,
    ProcessHeadersCallback callback) {
  if (!process_request_headers_) {
    std::move(callback).Run(
        base::unexpected(PlatformRuntimeStatus::kLibraryUnavailable));
    return;
  }

  HeaderCaptureContext context{.input = input_headers};

  const bool result = process_request_headers_(
      &context, &GetHeaderCallback, &SetHeaderCallback, /*url=*/nullptr);

  if (!result) {
    std::move(callback).Run(base::unexpected(PlatformRuntimeStatus::kFailure));
    return;
  }
  // A successful call does not imply the library produced anything, so an
  // empty output is reported separately rather than as success.
  if (context.output.IsEmpty()) {
    std::move(callback).Run(base::unexpected(PlatformRuntimeStatus::kNoOutput));
    return;
  }
  std::move(callback).Run(base::ok(std::move(context.output)));
}

PlatformRuntimeServiceImpl::PlatformRuntimeServiceImpl(
    mojo::PendingReceiver<
        request_header_integrity::mojom::PlatformRuntimeService> receiver)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      receiver_(this, std::move(receiver)) {}

PlatformRuntimeServiceImpl::~PlatformRuntimeServiceImpl() = default;

void PlatformRuntimeServiceImpl::LoadLibrary(
    const base::FilePath& library_path,
    mojo::PendingReceiver<request_header_integrity::mojom::PlatformRuntime>
        runtime) {
  runner_.emplace(task_runner_, library_path, std::move(runtime));
}

}  // namespace platform_runtime
