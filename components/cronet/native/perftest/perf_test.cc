// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/at_exit.h"
#include "base/atomic_sequence_num.h"
#include "base/check_op.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/cronet/native/test/test_upload_data_provider.h"
#include "components/cronet/native/test/test_url_request_callback.h"
#include "components/cronet/native/test/test_util.h"
#include "cronet_c.h"
#include "net/base/net_errors.h"
#include "net/cert/mock_cert_verifier.h"

namespace {

// Type of executor to use for a particular benchmark:
enum ExecutorType {
  EXECUTOR_DIRECT,  // Direct executor (on network thread).
  EXECUTOR_THREAD,  // Post to main thread.
};

// Upload or download benchmark.
enum Direction {
  DIRECTION_UP,
  DIRECTION_DOWN,
};

// Small or large benchmark payload.
enum Size {
  SIZE_LARGE,
  SIZE_SMALL,
};

// Protocol to benchmark.
enum Protocol {
  PROTOCOL_HTTP,
  PROTOCOL_QUIC,
};

// Dictionary of benchmark options.
std::unique_ptr<base::DictionaryValue> g_options;

// Return a string configuration option.
std::string GetConfigString(const char* key) {
  std::string value;
  CHECK(g_options->GetString(key, &value)) << "Cannot find key: " << key;
  return value;
}

// Return an int configuration option.
int GetConfigInt(const char* key) {
  int value;
  CHECK(g_options->GetInteger(key, &value)) << "Cannot find key: " << key;
  return value;
}

// Put together a benchmark configuration into a benchmark name.
// Make it fixed length for more readable tables.
// Benchmark names are written to the JSON output file and slurped up by
// Telemetry on the host.
std::string BuildBenchmarkName(ExecutorType executor,
                               Direction direction,
                               Protocol protocol,
                               int concurrency,
                               int iterations) {
  std::string name = direction == DIRECTION_UP ? "Up___" : "Down_";
  switch (protocol) {
    case PROTOCOL_HTTP:
      name += "H_";
      break;
    case PROTOCOL_QUIC:
      name += "Q_";
      break;
  }
  name += std::to_string(iterations) + "_" + std::to_string(concurrency) + "_";
  switch (executor) {
    case EXECUTOR_DIRECT:
      name += "ExDir";
      break;
    case EXECUTOR_THREAD:
      name += "ExThr";
      break;
  }
  return name;
}

// Cronet UploadDataProvider to use for benchmark.
class UploadDataProvider : public cronet::test::TestUploadDataProvider {
 public:
  // |length| indicates how many bytes to upload.
  UploadDataProvider(size_t length)
      : TestUploadDataProvider(cronet::test::TestUploadDataProvider::SYNC,
                               nullptr),
        length_(length),
        remaining_(length) {}

 private:
  int64_t GetLength() const override { return length_; }

  // Override of TestUploadDataProvider::Read() to simply report buffers filled.
  void Read(Cronet_UploadDataSinkPtr upload_data_sink,
            Cronet_BufferPtr buffer) override {
    CHECK(remaining_ > 0);
    size_t buffer_size = Cronet_Buffer_GetSize(buffer);
    size_t sending = std::min(buffer_size, remaining_);
    Cronet_UploadDataSink_OnReadSucceeded(upload_data_sink, sending, false);
    remaining_ -= sending;
  }

  const size_t length_;
  // Count of bytes remaining to be uploaded.
  size_t remaining_;
};

// Cronet UrlRequestCallback to use for benchmarking.
class Callback : public cronet::test::TestUrlRequestCallback {
 public:
  Callback()
      : TestUrlRequestCallback(true),
        task_runner_(base::ThreadTaskRunnerHandle::Get()) {}
  ~Callback() override { Cronet_UrlRequestCallback_Destroy(callback_); }

  // Start one repeated UrlRequest. |iterations_completed| is used to keep track
  // of how many requests have completed.  Final iteration should Quit()
  // |run_loop|.
  void Start(size_t buffer_size,
             int iterations,
             int concurrency,
             size_t length,
             const std::string& url,
             base::AtomicSequenceNumber* iterations_completed,
             Cronet_EnginePtr engine,
             ExecutorType executor,
             Direction direction,
             base::RunLoop* run_loop) {
    iterations_ = iterations;
    concurrency_ = concurrency;
    length_ = length;
    url_ = &url;
    iterations_completed_ = iterations_completed;
    engine_ = engine;
    callback_ = CreateUrlRequestCallback();
    CHECK(!executor_);
    switch (executor) {
      case EXECUTOR_DIRECT:
        // TestUrlRequestCallback(true) was called above, so parent will create
        // a direct executor.
        GetExecutor();
        break;
      case EXECUTOR_THREAD:
        // Create an executor that posts back to this thread.
        executor_ = Cronet_Executor_CreateWith(Callback::Execute);
        Cronet_Executor_SetClientContext(executor_, this);
        break;
    }
    CHECK(executor_);
    direction_ = direction;
    buffer_size_ = buffer_size;
    run_loop_ = run_loop;
    StartRequest();
  }

 private:
  // Create and start a UrlRequest.
  void StartRequest() {
    Cronet_UrlRequestPtr request = Cronet_UrlRequest_Create();
    Cronet_UrlRequestParamsPtr request_params =
        Cronet_UrlRequestParams_Create();
    if (direction_ == DIRECTION_UP) {
      // Create and set an UploadDataProvider on the UrlRequest.
      upload_data_provider_ = std::make_unique<UploadDataProvider>(length_);
      cronet_upload_data_provider_ =
          upload_data_provider_->CreateUploadDataProvider();
      Cronet_UrlRequestParams_upload_data_provider_set(
          request_params, cronet_upload_data_provider_);
      // Set Content-Type header.
      Cronet_HttpHeaderPtr header = Cronet_HttpHeader_Create();
      Cronet_HttpHeader_name_set(header, "Content-Type");
      Cronet_HttpHeader_value_set(header, "application/octet-stream");
      Cronet_UrlRequestParams_request_headers_add(request_params, header);
      Cronet_HttpHeader_Destroy(header);
    }
    Cronet_UrlRequest_InitWithParams(request, engine_, url_->c_str(),
                                     request_params, callback_, executor_);
    Cronet_UrlRequestParams_Destroy(request_params);
    Cronet_UrlRequest_Start(request);
  }

  void OnResponseStarted(Cronet_UrlRequestPtr request,
                         Cronet_UrlResponseInfoPtr info) override {
    CHECK_EQ(200, Cronet_UrlResponseInfo_http_status_code_get(info));
    response_step_ = ON_RESPONSE_STARTED;
    Cronet_BufferPtr buffer = Cronet_Buffer_Create();
    Cronet_Buffer_InitWithAlloc(buffer, buffer_size_);
    StartNextRead(request, buffer);
  }

  void OnSucceeded(Cronet_UrlRequestPtr request,
                   Cronet_UrlResponseInfoPtr info) override {
    Cronet_UrlRequest_Destroy(request);
    if (cronet_upload_data_provider_)
      Cronet_UploadDataProvider_Destroy(cronet_upload_data_provider_);

    int iteration = iterations_completed_->GetNext();
    // If this was the final iteration, quit the RunLoop.
    if (iteration == (iterations_ - 1))
      run_loop_->Quit();
    // Don't start another request if complete.
    if (iteration >= (iterations_ - concurrency_))
      return;
    // Start another request.
    StartRequest();
  }

  void OnFailed(Cronet_UrlRequestPtr request,
                Cronet_UrlResponseInfoPtr info,
                Cronet_ErrorPtr error) override {
    CHECK(false) << "Request failed with error "
                 << Cronet_Error_error_code_get(error);
  }

  // A simple executor that posts back to |task_runner_|.
  static void Execute(Cronet_ExecutorPtr self, Cronet_RunnablePtr runnable) {
    auto* callback =
        static_cast<Callback*>(Cronet_Executor_GetClientContext(self));
    callback->task_runner_->PostTask(
        FROM_HERE, cronet::test::RunnableWrapper::CreateOnceClosure(runnable));
  }

  Direction direction_;
  int iterations_;
  int concurrency_;
  size_t length_;
  const std::string* url_;
  base::AtomicSequenceNumber* iterations_completed_;
  Cronet_EnginePtr engine_;
  Cronet_UrlRequestCallbackPtr callback_;
  Cronet_UploadDataProviderPtr cronet_upload_data_provider_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::RunLoop* run_loop_;
  size_t buffer_size_;
  std::unique_ptr<UploadDataProvider> upload_data_provider_;
};

// An individual benchmark instance.
class Benchmark {
 public:
  ~Benchmark() { Cronet_Engine_Destroy(engine_); }

  // Run and time the benchmark.
  static void Run(ExecutorType executor,
                  Direction direction,
                  Size size,
                  Protocol protocol,
                  int concurrency,
                  base::DictionaryValue* results) {
    std::string resource;
    int iterations;
    size_t length;
    switch (size) {
      case SIZE_SMALL:
        resource = GetConfigString("SMALL_RESOURCE");
        iterations = GetConfigInt("SMALL_ITERATIONS");
        length = GetConfigInt("SMALL_RESOURCE_SIZE");
        break;
      case SIZE_LARGE:
        // When measuring a large upload, only download a small amount so
        // download time isn't significant.
        resource = GetConfigString(
            direction == DIRECTION_UP ? "SMALL_RESOURCE" : "LARGE_RESOURCE");
        iterations = GetConfigInt("LARGE_ITERATIONS");
        length = GetConfigInt("LARGE_RESOURCE_SIZE");
        break;
    }
    std::string name = BuildBenchmarkName(executor, direction, protocol,
                                          concurrency, iterations);
    std::string scheme;
    std::string host;
    int port;
    switch (protocol) {
      case PROTOCOL_HTTP:
        scheme = "http";
        host = GetConfigString("HOST_IP");
        port = GetConfigInt("HTTP_PORT");
        break;
      case PROTOCOL_QUIC:
        scheme = "https";
        host = GetConfigString("HOST");
        port = GetConfigInt("QUIC_PORT");
        break;
    }
    std::string url =
        scheme + "://" + host + ":" + std::to_string(port) + "/" + resource;
    size_t buffer_size = length > (size_t)GetConfigInt("MAX_BUFFER_SIZE")
                             ? GetConfigInt("MAX_BUFFER_SIZE")
                             : length;
    Benchmark(executor, direction, size, protocol, concurrency, iterations,
              length, buffer_size, name, url, host, port, results)
        .RunInternal();
  }

 private:
  Benchmark(ExecutorType executor,
            Direction direction,
            Size size,
            Protocol protocol,
            int concurrency,
            int iterations,
            size_t length,
            size_t buffer_size,
            const std::string& name,
            const std::string& url,
            const std::string& host,
            int port,
            base::DictionaryValue* results)
      : iterations_(iterations),
        concurrency_(concurrency),
        length_(length),
        buffer_size_(buffer_size),
        name_(name),
        url_(url),
        callbacks_(concurrency),
        executor_(executor),
        direction_(direction),
        results_(results) {
    Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
    // Add Host Resolver Rules.
    std::string host_resolver_rules =
        "MAP test.example.com " + GetConfigString("HOST_IP") + ",";
    Cronet_EngineParams_experimental_options_set(
        engine_params,
        base::StringPrintf(
            "{ \"HostResolverRules\": { \"host_resolver_rules\" : \"%s\" } }",
            host_resolver_rules.c_str())
            .c_str());
    // Create Cronet Engine.
    engine_ = Cronet_Engine_Create();
    if (protocol == PROTOCOL_QUIC) {
      Cronet_EngineParams_enable_quic_set(engine_params, true);
      // Set QUIC hint.
      Cronet_QuicHintPtr quic_hint = Cronet_QuicHint_Create();
      Cronet_QuicHint_host_set(quic_hint, host.c_str());
      Cronet_QuicHint_port_set(quic_hint, port);
      Cronet_QuicHint_alternate_port_set(quic_hint, port);
      Cronet_EngineParams_quic_hints_add(engine_params, quic_hint);
      Cronet_QuicHint_Destroy(quic_hint);
      // Set Mock Cert Verifier.
      auto cert_verifier = std::make_unique<net::MockCertVerifier>();
      cert_verifier->set_default_result(net::OK);
      Cronet_Engine_SetMockCertVerifierForTesting(engine_,
                                                  cert_verifier.release());
    }

    // Start Cronet Engine.
    Cronet_Engine_StartWithParams(engine_, engine_params);
    Cronet_EngineParams_Destroy(engine_params);
  }

  // Run and time the benchmark.
  void RunInternal() {
    base::RunLoop run_loop;
    base::TimeTicks start_time = base::TimeTicks::Now();
    // Start all concurrent requests.
    for (auto& callback : callbacks_) {
      callback.Start(buffer_size_, iterations_, concurrency_, length_, url_,
                     &iterations_completed_, engine_, executor_, direction_,
                     &run_loop);
    }
    run_loop.Run();
    base::TimeDelta run_time = base::TimeTicks::Now() - start_time;
    results_->SetInteger(name_, static_cast<int>(run_time.InMilliseconds()));
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const int iterations_;
  const int concurrency_;
  const size_t length_;
  const size_t buffer_size_;
  const std::string name_;
  const std::string url_;
  std::vector<Callback> callbacks_;
  base::AtomicSequenceNumber iterations_completed_;
  Cronet_EnginePtr engine_;
  const ExecutorType executor_;
  const Direction direction_;
  base::DictionaryValue* const results_;
};

}  // namespace

void PerfTest(const char* json_args) {
  base::AtExitManager exit_manager;

  // Parse benchmark options into |g_options|.
  std::string benchmark_options = json_args;
  std::unique_ptr<base::Value> options_value =
      base::JSONReader::ReadDeprecated(benchmark_options);
  CHECK(options_value) << "Parsing benchmark options failed: "
                       << benchmark_options;
  g_options = base::DictionaryValue::From(std::move(options_value));
  CHECK(g_options) << "Benchmark options string is not a dictionary: "
                   << benchmark_options
                   << " See DEFAULT_BENCHMARK_CONFIG in perf_test_util.py.";

  // Run benchmarks putting timing results into |results|.
  base::DictionaryValue results;
  for (ExecutorType executor : {EXECUTOR_DIRECT, EXECUTOR_THREAD}) {
    for (Direction direction : {DIRECTION_DOWN, DIRECTION_UP}) {
      for (Protocol protocol : {PROTOCOL_HTTP, PROTOCOL_QUIC}) {
        // Run large and small benchmarks one at a time to test single-threaded
        // use. Also run them four at a time to see how they benefit from
        // concurrency. The value four was chosen as many devices are now
        // quad-core.
        Benchmark::Run(executor, direction, SIZE_LARGE, protocol, 1, &results);
        Benchmark::Run(executor, direction, SIZE_LARGE, protocol, 4, &results);
        Benchmark::Run(executor, direction, SIZE_SMALL, protocol, 1, &results);
        Benchmark::Run(executor, direction, SIZE_SMALL, protocol, 4, &results);
        // Large benchmarks are generally bandwidth bound and unaffected by
        // per-request overhead.  Small benchmarks are not, so test at
        // further increased concurrency to see if further benefit is possible.
        Benchmark::Run(executor, direction, SIZE_SMALL, protocol, 8, &results);
      }
    }
  }

  // Write |results| into results file.
  std::string results_string;
  base::JSONWriter::Write(results, &results_string);
  FILE* results_file = fopen(GetConfigString("RESULTS_FILE").c_str(), "wb");
  fwrite(results_string.c_str(), results_string.length(), 1, results_file);
  fclose(results_file);
  fclose(fopen(GetConfigString("DONE_FILE").c_str(), "wb"));
}
