// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_ENGINE_H_
#define COMPONENTS_CRONET_NATIVE_ENGINE_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "components/cronet/native/generated/cronet.idl_impl_interface.h"

extern "C" typedef struct stream_engine stream_engine;

namespace net {
class CertVerifier;
}

namespace cronet {
class CronetURLRequestContext;

// Implementation of Cronet_Engine that uses CronetURLRequestContext.
class Cronet_EngineImpl : public Cronet_Engine {
 public:
  Cronet_EngineImpl();
  ~Cronet_EngineImpl() override;

  // Cronet_Engine implementation:
  Cronet_RESULT StartWithParams(Cronet_EngineParamsPtr params) override
      LOCKS_EXCLUDED(lock_);
  bool StartNetLogToFile(Cronet_String file_name, bool log_all) override
      LOCKS_EXCLUDED(lock_);
  void StopNetLog() override LOCKS_EXCLUDED(lock_);
  Cronet_String GetVersionString() override;
  Cronet_String GetDefaultUserAgent() override;
  Cronet_RESULT Shutdown() override LOCKS_EXCLUDED(lock_);
  void AddRequestFinishedListener(
      Cronet_RequestFinishedInfoListenerPtr listener,
      Cronet_ExecutorPtr executor) override;
  void RemoveRequestFinishedListener(
      Cronet_RequestFinishedInfoListenerPtr listener) override;

  // Check |result| and aborts if result is not SUCCESS and enableCheckResult
  // is true.
  Cronet_RESULT CheckResult(Cronet_RESULT result);

  // Set Mock CertVerifier for testing. Must be called before StartWithParams.
  void SetMockCertVerifierForTesting(
      std::unique_ptr<net::CertVerifier> mock_cert_verifier);

  // Get stream engine for GRPC Bidirectional Stream support. The returned
  // stream_engine is owned by |this| and is only valid until |this| shutdown.
  stream_engine* GetBidirectionalStreamEngine();

  CronetURLRequestContext* cronet_url_request_context() const {
    return context_.get();
  }

  // Returns true if there is a listener currently registered (using
  // AddRequestFinishedListener()), and false otherwise.
  bool HasRequestFinishedListener();

  // Provide |request_info| to all registered RequestFinishedListeners.
  void ReportRequestFinished(
      scoped_refptr<base::RefCountedData<Cronet_RequestFinishedInfo>>
          request_info,
      scoped_refptr<base::RefCountedData<Cronet_UrlResponseInfo>>
          url_response_info,
      scoped_refptr<base::RefCountedData<Cronet_Error>> error);

 private:
  class StreamEngineImpl;
  class Callback;

  // Enable runtime CHECK of the result.
  bool enable_check_result_ = true;

  // Synchronize access to member variables from different threads.
  base::Lock lock_;
  // Cronet URLRequest context used for all network operations.
  std::unique_ptr<CronetURLRequestContext> context_;
  // Signaled when |context_| initialization is done.
  base::WaitableEvent init_completed_;

  // Flag that indicates whether logging is in progress.
  bool is_logging_ GUARDED_BY(lock_) = false;
  // Signaled when |StopNetLog| is done.
  base::WaitableEvent stop_netlog_completed_;

  // Storage path used by this engine.
  std::string in_use_storage_path_ GUARDED_BY(lock_);

  // Stream engine for GRPC Bidirectional Stream support.
  std::unique_ptr<StreamEngineImpl> stream_engine_;

  // Mock CertVerifier for testing. Only valid until StartWithParams.
  std::unique_ptr<net::CertVerifier> mock_cert_verifier_;

  // Stores registered RequestFinishedInfoListeners with their associated
  // Executors.
  base::flat_map<Cronet_RequestFinishedInfoListenerPtr, Cronet_ExecutorPtr>
      request_finished_registrations_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(Cronet_EngineImpl);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_NATIVE_ENGINE_H_
