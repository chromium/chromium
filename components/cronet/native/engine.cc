// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/engine.h"

#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/cronet/cronet_global_state.h"
#include "components/cronet/cronet_url_request_context.h"
#include "components/cronet/native/generated/cronet.idl_impl_struct.h"
#include "components/cronet/native/include/cronet_c.h"
#include "components/cronet/native/runnables.h"
#include "components/cronet/url_request_context_config.h"
#include "components/cronet/version.h"
#include "components/grpc_support/include/bidirectional_stream_c.h"
#include "net/base/hash_value.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

class SharedEngineState {
 public:
  SharedEngineState()
      : default_user_agent_(cronet::CreateDefaultUserAgent(CRONET_VERSION)) {}

  // Marks |storage_path| in use, so multiple engines would not use it at the
  // same time. Returns |true| if marked successfully, |false| if it is in use
  // by another engine.
  bool MarkStoragePathInUse(const std::string& storage_path)
      LOCKS_EXCLUDED(lock_) {
    base::AutoLock lock(lock_);
    return in_use_storage_paths_.emplace(storage_path).second;
  }

  // Unmarks |storage_path| in use, so another engine could use it.
  void UnmarkStoragePathInUse(const std::string& storage_path)
      LOCKS_EXCLUDED(lock_) {
    base::AutoLock lock(lock_);
    in_use_storage_paths_.erase(storage_path);
  }

  // Returns default user agent, based on Cronet version, application info and
  // platform-specific additional information.
  Cronet_String GetDefaultUserAgent() const {
    return default_user_agent_.c_str();
  }

  static SharedEngineState* GetInstance();

 private:
  const std::string default_user_agent_;
  // Protecting shared state.
  base::Lock lock_;
  std::unordered_set<std::string> in_use_storage_paths_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(SharedEngineState);
};

SharedEngineState* SharedEngineState::GetInstance() {
  static base::NoDestructor<SharedEngineState> instance;
  return instance.get();
}

// Does basic validation of host name for PKP and returns |true| if
// host is valid.
bool IsValidHostnameForPkp(const std::string& host) {
  if (host.empty())
    return false;
  if (host.size() > 255)
    return false;
  if (host.find_first_of(":\\/=\'\",") != host.npos)
    return false;
  return true;
}

}  // namespace

namespace cronet {

Cronet_EngineImpl::Cronet_EngineImpl()
    : init_completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      stop_netlog_completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED) {}

Cronet_EngineImpl::~Cronet_EngineImpl() {
  Shutdown();
}

Cronet_RESULT Cronet_EngineImpl::StartWithParams(
    Cronet_EngineParamsPtr params) {
  cronet::EnsureInitialized();
  base::AutoLock lock(lock_);

  enable_check_result_ = params->enable_check_result;
  if (context_) {
    return CheckResult(Cronet_RESULT_ILLEGAL_STATE_ENGINE_ALREADY_STARTED);
  }

  URLRequestContextConfigBuilder context_config_builder;
  context_config_builder.enable_quic = params->enable_quic;
  context_config_builder.enable_spdy = params->enable_http2;
  context_config_builder.enable_brotli = params->enable_brotli;
  switch (params->http_cache_mode) {
    case Cronet_EngineParams_HTTP_CACHE_MODE_DISABLED:
      context_config_builder.http_cache = URLRequestContextConfig::DISABLED;
      break;
    case Cronet_EngineParams_HTTP_CACHE_MODE_IN_MEMORY:
      context_config_builder.http_cache = URLRequestContextConfig::MEMORY;
      break;
    case Cronet_EngineParams_HTTP_CACHE_MODE_DISK: {
      context_config_builder.http_cache = URLRequestContextConfig::DISK;
#if defined(OS_WIN)
      const base::FilePath storage_path(
          base::FilePath::FromUTF8Unsafe(params->storage_path));
#else
      const base::FilePath storage_path(params->storage_path);
#endif
      if (!base::DirectoryExists(storage_path)) {
        return CheckResult(
            Cronet_RESULT_ILLEGAL_ARGUMENT_STORAGE_PATH_MUST_EXIST);
      }
      if (!SharedEngineState::GetInstance()->MarkStoragePathInUse(
              params->storage_path)) {
        LOG(ERROR) << "Disk cache path " << params->storage_path
                   << " is already used, cache disabled.";
        return CheckResult(Cronet_RESULT_ILLEGAL_STATE_STORAGE_PATH_IN_USE);
      }
      in_use_storage_path_ = params->storage_path;
      break;
    }
    default:
      context_config_builder.http_cache = URLRequestContextConfig::DISABLED;
  }
  context_config_builder.http_cache_max_size = params->http_cache_max_size;
  context_config_builder.storage_path = params->storage_path;
  context_config_builder.accept_language = params->accept_language;
  context_config_builder.user_agent = params->user_agent;
  context_config_builder.experimental_options = params->experimental_options;
  context_config_builder.bypass_public_key_pinning_for_local_trust_anchors =
      params->enable_public_key_pinning_bypass_for_local_trust_anchors;
  if (!isnan(params->network_thread_priority)) {
    context_config_builder.network_thread_priority =
        params->network_thread_priority;
  }

  // MockCertVerifier to use for testing purposes.
  context_config_builder.mock_cert_verifier = std::move(mock_cert_verifier_);
  std::unique_ptr<URLRequestContextConfig> config =
      context_config_builder.Build();

  for (const auto& public_key_pins : params->public_key_pins) {
    auto pkp = std::make_unique<URLRequestContextConfig::Pkp>(
        public_key_pins.host, public_key_pins.include_subdomains,
        base::Time::FromJavaTime(public_key_pins.expiration_date));
    if (pkp->host.empty())
      return CheckResult(Cronet_RESULT_NULL_POINTER_HOSTNAME);
    if (!IsValidHostnameForPkp(pkp->host))
      return CheckResult(Cronet_RESULT_ILLEGAL_ARGUMENT_INVALID_HOSTNAME);
    if (pkp->expiration_date.is_null())
      return CheckResult(Cronet_RESULT_NULL_POINTER_EXPIRATION_DATE);
    if (public_key_pins.pins_sha256.empty())
      return CheckResult(Cronet_RESULT_NULL_POINTER_SHA256_PINS);
    for (const auto& pin_sha256 : public_key_pins.pins_sha256) {
      net::HashValue pin_hash;
      if (!pin_hash.FromString(pin_sha256))
        return CheckResult(Cronet_RESULT_ILLEGAL_ARGUMENT_INVALID_PIN);
      pkp->pin_hashes.push_back(pin_hash);
    }
    config->pkp_list.push_back(std::move(pkp));
  }

  for (const auto& quic_hint : params->quic_hints) {
    config->quic_hints.push_back(
        std::make_unique<URLRequestContextConfig::QuicHint>(
            quic_hint.host, quic_hint.port, quic_hint.alternate_port));
  }

  context_ = std::make_unique<CronetURLRequestContext>(
      std::move(config), std::make_unique<Callback>(this));

  // TODO(mef): It'd be nice to remove the java code and this code, and get
  // rid of CronetURLRequestContextAdapter::InitRequestContextOnInitThread.
  // Could also make CronetURLRequestContext::InitRequestContextOnInitThread()
  // private and mark CronetLibraryLoader.postToInitThread() as
  // @VisibleForTesting (as the only external use will be in a test).

  // Initialize context on the init thread.
  cronet::PostTaskToInitThread(
      FROM_HERE,
      base::BindOnce(&CronetURLRequestContext::InitRequestContextOnInitThread,
                     base::Unretained(context_.get())));
  return CheckResult(Cronet_RESULT_SUCCESS);
}

bool Cronet_EngineImpl::StartNetLogToFile(Cronet_String file_name,
                                          bool log_all) {
  base::AutoLock lock(lock_);
  if (is_logging_ || !context_)
    return false;
  is_logging_ = context_->StartNetLogToFile(file_name, log_all);
  return is_logging_;
}

void Cronet_EngineImpl::StopNetLog() {
  {
    base::AutoLock lock(lock_);
    if (!is_logging_ || !context_)
      return;
    context_->StopNetLog();
    // Release |lock| so it could be acquired in OnStopNetLog.
  }
  stop_netlog_completed_.Wait();
  stop_netlog_completed_.Reset();
}

Cronet_String Cronet_EngineImpl::GetVersionString() {
  return CRONET_VERSION;
}

Cronet_String Cronet_EngineImpl::GetDefaultUserAgent() {
  return SharedEngineState::GetInstance()->GetDefaultUserAgent();
}

Cronet_RESULT Cronet_EngineImpl::Shutdown() {
  {  // Check whether engine is running.
    base::AutoLock lock(lock_);
    if (!context_)
      return CheckResult(Cronet_RESULT_SUCCESS);
  }
  // Wait for init to complete on init and network thread (without lock, so
  // other thread could access it).
  init_completed_.Wait();
  // If not logging, this is a no-op.
  StopNetLog();
  // Stop the engine.
  base::AutoLock lock(lock_);
  if (context_->IsOnNetworkThread()) {
    return CheckResult(
        Cronet_RESULT_ILLEGAL_STATE_CANNOT_SHUTDOWN_ENGINE_FROM_NETWORK_THREAD);
  }

  if (!in_use_storage_path_.empty()) {
    SharedEngineState::GetInstance()->UnmarkStoragePathInUse(
        in_use_storage_path_);
  }

  stream_engine_.reset();
  context_.reset();
  return CheckResult(Cronet_RESULT_SUCCESS);
}

void Cronet_EngineImpl::AddRequestFinishedListener(
    Cronet_RequestFinishedInfoListenerPtr listener,
    Cronet_ExecutorPtr executor) {
  if (listener == nullptr || executor == nullptr) {
    LOG(DFATAL) << "Both listener and executor must be non-null. listener: "
                << listener << " executor: " << executor << ".";
    return;
  }
  base::AutoLock lock(lock_);
  if (request_finished_registrations_.count(listener) > 0) {
    LOG(DFATAL) << "Listener " << listener
                << " already registered with executor "
                << request_finished_registrations_[listener]
                << ", *NOT* changing to new executor " << executor << ".";
    return;
  }
  request_finished_registrations_.insert({listener, executor});
}

void Cronet_EngineImpl::RemoveRequestFinishedListener(
    Cronet_RequestFinishedInfoListenerPtr listener) {
  base::AutoLock lock(lock_);
  if (request_finished_registrations_.erase(listener) != 1) {
    LOG(DFATAL) << "Asked to erase non-existent RequestFinishedInfoListener "
                << listener << ".";
  }
}

namespace {

using RequestFinishedInfo = base::RefCountedData<Cronet_RequestFinishedInfo>;
using UrlResponseInfo = base::RefCountedData<Cronet_UrlResponseInfo>;
using CronetError = base::RefCountedData<Cronet_Error>;

template <typename T>
T* GetData(scoped_refptr<base::RefCountedData<T>> ptr) {
  return ptr == nullptr ? nullptr : &ptr->data;
}

}  // namespace

void Cronet_EngineImpl::ReportRequestFinished(
    scoped_refptr<RequestFinishedInfo> request_info,
    scoped_refptr<UrlResponseInfo> url_response_info,
    scoped_refptr<CronetError> error) {
  base::flat_map<Cronet_RequestFinishedInfoListenerPtr, Cronet_ExecutorPtr>
      registrations;
  {
    base::AutoLock lock(lock_);
    // We copy under to avoid calling callbacks (which may run on direct
    // executors and call Engine methods) with the lock held.
    //
    // The map holds only pointers and shouldn't be very large.
    registrations = request_finished_registrations_;
  }
  for (auto& pair : registrations) {
    auto* request_finished_listener = pair.first;
    auto* request_finished_executor = pair.second;

    request_finished_executor->Execute(
        new cronet::OnceClosureRunnable(base::BindOnce(
            [](Cronet_RequestFinishedInfoListenerPtr request_finished_listener,
               scoped_refptr<RequestFinishedInfo> request_info,
               scoped_refptr<UrlResponseInfo> url_response_info,
               scoped_refptr<CronetError> error) {
              request_finished_listener->OnRequestFinished(
                  GetData(request_info), GetData(url_response_info),
                  GetData(error));
            },
            request_finished_listener, request_info, url_response_info,
            error)));
  }
}

Cronet_RESULT Cronet_EngineImpl::CheckResult(Cronet_RESULT result) {
  if (enable_check_result_)
    CHECK_EQ(Cronet_RESULT_SUCCESS, result);
  return result;
}

bool Cronet_EngineImpl::HasRequestFinishedListener() {
  base::AutoLock lock(lock_);
  return request_finished_registrations_.size() > 0;
}

// The struct stream_engine for grpc support.
// Holds net::URLRequestContextGetter and app-specific annotation.
class Cronet_EngineImpl::StreamEngineImpl : public stream_engine {
 public:
  explicit StreamEngineImpl(net::URLRequestContextGetter* context_getter) {
    context_getter_ = context_getter;
    obj = context_getter_.get();
    annotation = nullptr;
  }

  ~StreamEngineImpl() {
    obj = nullptr;
    annotation = nullptr;
  }

 private:
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
};

// Callback is owned by CronetURLRequestContext. It is invoked and deleted
// on the network thread.
class Cronet_EngineImpl::Callback : public CronetURLRequestContext::Callback {
 public:
  explicit Callback(Cronet_EngineImpl* engine);
  ~Callback() override;

  // CronetURLRequestContext::Callback implementation:
  void OnInitNetworkThread() override LOCKS_EXCLUDED(engine_->lock_);
  void OnDestroyNetworkThread() override;
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType effective_connection_type) override;
  void OnRTTOrThroughputEstimatesComputed(
      int32_t http_rtt_ms,
      int32_t transport_rtt_ms,
      int32_t downstream_throughput_kbps) override;
  void OnRTTObservation(int32_t rtt_ms,
                        int32_t timestamp_ms,
                        net::NetworkQualityObservationSource source) override;
  void OnThroughputObservation(
      int32_t throughput_kbps,
      int32_t timestamp_ms,
      net::NetworkQualityObservationSource source) override;
  void OnStopNetLogCompleted() override LOCKS_EXCLUDED(engine_->lock_);

 private:
  // The engine which owns context that owns |this| callback.
  Cronet_EngineImpl* const engine_;

  // All methods are invoked on the network thread.
  THREAD_CHECKER(network_thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(Callback);
};

Cronet_EngineImpl::Callback::Callback(Cronet_EngineImpl* engine)
    : engine_(engine) {
  DETACH_FROM_THREAD(network_thread_checker_);
}

Cronet_EngineImpl::Callback::~Callback() = default;

void Cronet_EngineImpl::Callback::OnInitNetworkThread() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  // It is possible that engine_->context_ is reset from main thread while
  // being intialized on network thread.
  base::AutoLock lock(engine_->lock_);
  if (engine_->context_) {
    // Initialize bidirectional stream engine for grpc.
    engine_->stream_engine_ = std::make_unique<StreamEngineImpl>(
        engine_->context_->CreateURLRequestContextGetter());
    engine_->init_completed_.Signal();
  }
}

void Cronet_EngineImpl::Callback::OnDestroyNetworkThread() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  DCHECK(!engine_->stream_engine_);
}

void Cronet_EngineImpl::Callback::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  NOTIMPLEMENTED();
}

void Cronet_EngineImpl::Callback::OnRTTOrThroughputEstimatesComputed(
    int32_t http_rtt_ms,
    int32_t transport_rtt_ms,
    int32_t downstream_throughput_kbps) {
  NOTIMPLEMENTED();
}

void Cronet_EngineImpl::Callback::OnRTTObservation(
    int32_t rtt_ms,
    int32_t timestamp_ms,
    net::NetworkQualityObservationSource source) {
  NOTIMPLEMENTED();
}

void Cronet_EngineImpl::Callback::OnThroughputObservation(
    int32_t throughput_kbps,
    int32_t timestamp_ms,
    net::NetworkQualityObservationSource source) {
  NOTIMPLEMENTED();
}

void Cronet_EngineImpl::Callback::OnStopNetLogCompleted() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  CHECK(engine_);
  base::AutoLock lock(engine_->lock_);
  DCHECK(engine_->is_logging_);
  engine_->is_logging_ = false;
  engine_->stop_netlog_completed_.Signal();
}

void Cronet_EngineImpl::SetMockCertVerifierForTesting(
    std::unique_ptr<net::CertVerifier> mock_cert_verifier) {
  CHECK(!context_);
  mock_cert_verifier_ = std::move(mock_cert_verifier);
}

stream_engine* Cronet_EngineImpl::GetBidirectionalStreamEngine() {
  init_completed_.Wait();
  return stream_engine_.get();
}

}  // namespace cronet

CRONET_EXPORT Cronet_EnginePtr Cronet_Engine_Create() {
  return new cronet::Cronet_EngineImpl();
}

CRONET_EXPORT void Cronet_Engine_SetMockCertVerifierForTesting(
    Cronet_EnginePtr engine,
    void* raw_mock_cert_verifier) {
  cronet::Cronet_EngineImpl* engine_impl =
      static_cast<cronet::Cronet_EngineImpl*>(engine);
  std::unique_ptr<net::CertVerifier> cert_verifier;
  cert_verifier.reset(static_cast<net::CertVerifier*>(raw_mock_cert_verifier));
  engine_impl->SetMockCertVerifierForTesting(std::move(cert_verifier));
}

CRONET_EXPORT stream_engine* Cronet_Engine_GetStreamEngine(
    Cronet_EnginePtr engine) {
  cronet::Cronet_EngineImpl* engine_impl =
      static_cast<cronet::Cronet_EngineImpl*>(engine);
  return engine_impl->GetBidirectionalStreamEngine();
}
