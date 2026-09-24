// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PLATFORM_RUNTIME_PLATFORM_RUNTIME_SERVICE_IMPL_H_
#define CHROME_SERVICES_PLATFORM_RUNTIME_PLATFORM_RUNTIME_SERVICE_IMPL_H_

#include <cstddef>

#include "base/memory/scoped_refptr.h"
#include "base/scoped_native_library.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "chrome/common/request_header_integrity/platform_runtime.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/http/http_request_headers.h"

namespace base {
class FilePath;
}

namespace platform_runtime {

// Runs the Platform Runtime native library inside a sandboxed utility process.
//
// This is the only component in Chrome that loads or executes the library.
// The service is generic: it loads the library once and forwards request
// headers to it for processing, returning whatever the library wrote back
// without inspecting or interpreting their contents.
class PlatformRuntimeServiceImpl
    : public request_header_integrity::mojom::PlatformRuntimeService {
 public:
  explicit PlatformRuntimeServiceImpl(
      mojo::PendingReceiver<
          request_header_integrity::mojom::PlatformRuntimeService> receiver);

  PlatformRuntimeServiceImpl(const PlatformRuntimeServiceImpl&) = delete;
  PlatformRuntimeServiceImpl& operator=(const PlatformRuntimeServiceImpl&) =
      delete;

  ~PlatformRuntimeServiceImpl() override;

  // request_header_integrity::mojom::PlatformRuntimeService:
  void LoadLibrary(
      const base::FilePath& library_path,
      mojo::PendingReceiver<request_header_integrity::mojom::PlatformRuntime>
          runtime) override;

 private:
  // Owns the loaded library and serves `mojom::PlatformRuntime` on a sequence
  // that allows blocking.
  class Runner : public request_header_integrity::mojom::PlatformRuntime {
   public:
    Runner(
        const base::FilePath& library_path,
        mojo::PendingReceiver<request_header_integrity::mojom::PlatformRuntime>
            receiver);
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;
    ~Runner() override;

    // request_header_integrity::mojom::PlatformRuntime:
    void ProcessHeaders(const net::HttpRequestHeaders& input_headers,
                        ProcessHeadersCallback callback) override;

   private:
    using GetHeaderFunction = bool (*)(void* ctx,
                                       const char* name,
                                       char* value_buf,
                                       size_t value_buf_size);
    using SetHeaderFunction = void (*)(void* ctx,
                                       const char* name,
                                       const char* value);
    using ProcessRequestHeadersFunction = bool (*)(void* ctx,
                                                   GetHeaderFunction get_header,
                                                   SetHeaderFunction set_header,
                                                   const char* url);

    base::ScopedNativeLibrary library_;
    ProcessRequestHeadersFunction process_request_headers_ = nullptr;
    mojo::Receiver<request_header_integrity::mojom::PlatformRuntime> receiver_;
  };

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::SequenceBound<Runner> runner_;

  mojo::Receiver<request_header_integrity::mojom::PlatformRuntimeService>
      receiver_;
};

}  // namespace platform_runtime

#endif  // CHROME_SERVICES_PLATFORM_RUNTIME_PLATFORM_RUNTIME_SERVICE_IMPL_H_
