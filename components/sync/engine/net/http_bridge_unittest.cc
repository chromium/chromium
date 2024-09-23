// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/net/http_bridge.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace syncer {

namespace {

using ::testing::HasSubstr;

// TODO(timsteele): Should use PathService here. See Chromium Issue 3113.
const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

}  // namespace

const char kUserAgent[] = "user-agent";

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SyncHttpBridgeTest DISABLED_SyncHttpBridgeTest
#else
#define MAYBE_SyncHttpBridgeTest SyncHttpBridgeTest
#endif  // BUILDFLAG(IS_ANDROID)
class MAYBE_SyncHttpBridgeTest : public testing::Test {
 public:
  MAYBE_SyncHttpBridgeTest() : io_thread_("IO thread") {
    test_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
  }

  void SetUp() override {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    io_thread_.StartWithOptions(std::move(options));
  }

  void TearDown() override { io_thread_.Stop(); }

  scoped_refptr<HttpBridge> BuildBridge() {
    return base::MakeRefCounted<CustomHttpBridge>(io_thread_.task_runner());
  }

  // Used by AbortAndReleaseBeforeFetchCompletes to test an interesting race
  // condition.
  void RunSyncThreadBridgeUseTest(base::WaitableEvent* signal_when_created,
                                  base::WaitableEvent* signal_when_released);

  scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() {
    return io_thread_.task_runner();
  }

  net::EmbeddedTestServer test_server_;

  base::Thread* io_thread() { return &io_thread_; }

  HttpBridge* bridge_for_race_test() { return bridge_for_race_test_; }

 private:
  // A custom HttpBridge implementation that sets a SharedURLLoaderFactory
  // instance from the IO-capable thread.
  class CustomHttpBridge : public HttpBridge {
   public:
    explicit CustomHttpBridge(
        scoped_refptr<base::SequencedTaskRunner> network_task_runner)
        : HttpBridge(kUserAgent,
                     network_task_runner,
                     /*pending_url_loader_factory=*/nullptr) {}

   protected:
    ~CustomHttpBridge() override = default;

    scoped_refptr<network::SharedURLLoaderFactory> CreateSharedURLLoader()
        override {
      return base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
    }
  };

  raw_ptr<HttpBridge> bridge_for_race_test_ = nullptr;

  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  // Separate thread for IO used by the HttpBridge.
  base::Thread io_thread_;
};

// An HttpBridge that doesn't actually make network requests and just calls
// back with fake response info.
// TODO(tim): Instead of inheriting here we should inject a component
// responsible for the MakeAsynchronousPost bit.
class ShuntedHttpBridge : public HttpBridge {
 public:
  // If |never_finishes| is true, the simulated request never actually
  // returns.
  ShuntedHttpBridge(MAYBE_SyncHttpBridgeTest* test, bool never_finishes)
      : HttpBridge(kUserAgent,
                   test->io_thread()->task_runner(),
                   /*pending_url_loader_factory=*/nullptr),
        test_(test),
        never_finishes_(never_finishes) {}

 protected:
  void MakeAsynchronousPost() override {
    ASSERT_TRUE(test_->GetIOThreadTaskRunner()->BelongsToCurrentThread());
    if (never_finishes_) {
      return;
    }

    // We don't actually want to make a request for this test, so just callback
    // as if it completed.
    test_->GetIOThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ShuntedHttpBridge::CallOnURLFetchComplete, this));
  }

 private:
  ~ShuntedHttpBridge() override = default;

  void CallOnURLFetchComplete() {
    ASSERT_TRUE(test_->GetIOThreadTaskRunner()->BelongsToCurrentThread());

    // Set up a fake content response.
    OnURLLoadCompleteInternal(200, net::OK, GURL("http://www.google.com"),
                              std::make_unique<std::string>("success!"));
  }
  const raw_ptr<MAYBE_SyncHttpBridgeTest> test_;
  bool never_finishes_;
};

void MAYBE_SyncHttpBridgeTest::RunSyncThreadBridgeUseTest(
    base::WaitableEvent* signal_when_created,
    base::WaitableEvent* signal_when_released) {
  {
    scoped_refptr<ShuntedHttpBridge> bridge(new ShuntedHttpBridge(this, true));
    bridge->SetURL(GURL("http://www.google.com:9999"));
    bridge->SetPostPayload("text/plain", 2, " ");
    bridge_for_race_test_ = bridge.get();
    signal_when_created->Signal();

    int os_error = 0;
    int response_code = 0;
    bridge->MakeSynchronousPost(&os_error, &response_code);
    bridge_for_race_test_ = nullptr;
  }
  signal_when_released->Signal();
}

// Test the HttpBridge without actually making any network requests.
TEST_F(MAYBE_SyncHttpBridgeTest, TestMakeSynchronousPostShunted) {
  scoped_refptr<HttpBridge> http_bridge(new ShuntedHttpBridge(this, false));
  http_bridge->SetURL(GURL("http://www.google.com:9999"));
  http_bridge->SetPostPayload("text/plain", 2, " ");

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  EXPECT_EQ(8, http_bridge->GetResponseContentLength());
  EXPECT_EQ(std::string("success!"),
            std::string(http_bridge->GetResponseContent()));
}

// Full round-trip test of the HttpBridge with compressed data, check if the
// data is correctly compressed.
TEST_F(MAYBE_SyncHttpBridgeTest, CompressedRequestPayloadCheck) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  std::string payload =
      "this should be echoed back, this should be echoed back.";
  http_bridge->SetURL(test_server_.GetURL("/echo"));
  http_bridge->SetPostPayload("application/x-www-form-urlencoded",
                              payload.length(), payload.c_str());
  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  // Verifying compression, check if the actual payload is compressed correctly.
  EXPECT_GT(payload.length(),
            static_cast<size_t>(http_bridge->GetResponseContentLength()));
  std::string compressed_payload(http_bridge->GetResponseContent(),
                                 http_bridge->GetResponseContentLength());
  std::string uncompressed_payload;
  compression::GzipUncompress(compressed_payload, &uncompressed_payload);
  EXPECT_EQ(payload, uncompressed_payload);
}

// Full round-trip test of the HttpBridge with compression, check if header
// fields("Content-Encoding" ,"Accept-Encoding" and user agent) are set
// correctly.
TEST_F(MAYBE_SyncHttpBridgeTest, CompressedRequestHeaderCheck) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  http_bridge->SetURL(test_server_.GetURL("/echoall"));

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  std::string response(http_bridge->GetResponseContent(),
                       http_bridge->GetResponseContentLength());
  EXPECT_THAT(response, HasSubstr("Content-Encoding: gzip"));
  EXPECT_THAT(response, HasSubstr(base::StringPrintf(
                            "%s: %s", net::HttpRequestHeaders::kAcceptEncoding,
                            "gzip, deflate")));
  EXPECT_NE(std::string::npos,
            response.find(base::StringPrintf(
                "%s: %s", net::HttpRequestHeaders::kUserAgent, kUserAgent)));
}

TEST_F(MAYBE_SyncHttpBridgeTest, TestExtraRequestHeaders) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  http_bridge->SetURL(test_server_.GetURL("/echoall"));
  http_bridge->SetExtraRequestHeaders("test:fnord");

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  std::string response(http_bridge->GetResponseContent(),
                       http_bridge->GetResponseContentLength());

  EXPECT_NE(std::string::npos, response.find("fnord"));
}

TEST_F(MAYBE_SyncHttpBridgeTest, TestResponseHeader) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  http_bridge->SetURL(test_server_.GetURL("/echoall"));

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  EXPECT_EQ(http_bridge->GetResponseHeaderValue("Content-type"), "text/html");
  EXPECT_TRUE(http_bridge->GetResponseHeaderValue("invalid-header").empty());
}

TEST_F(MAYBE_SyncHttpBridgeTest, HttpErrors) {
  ASSERT_TRUE(test_server_.Start());

  auto function = base::BindLambdaForTesting([&](net::HttpStatusCode code) {
    scoped_refptr<HttpBridge> http_bridge(BuildBridge());

    http_bridge->SetURL(test_server_.GetURL(std::string("/echo?status=") +
                                            base::NumberToString(code)));

    std::string test_payload = "###TEST PAYLOAD###";
    http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                                test_payload.c_str());

    int os_error = 0;
    int response_code = 0;
    bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
    EXPECT_TRUE(success) << "Sync failed for HTTP code status " << code;
    EXPECT_EQ(code, response_code);
    EXPECT_EQ(0, os_error);
  });

  // A sample collection of HTTP errors that can be relevant to sync engine.
  net::HttpStatusCode http_errors[] = {
      net::HTTP_BAD_REQUEST,
      net::HTTP_UNAUTHORIZED,
      net::HTTP_NOT_FOUND,
      net::HTTP_CONFLICT,
      net::HTTP_INTERNAL_SERVER_ERROR,
      net::HTTP_BAD_GATEWAY,
      net::HTTP_SERVICE_UNAVAILABLE,
      net::HTTP_GATEWAY_TIMEOUT,
  };
  for (net::HttpStatusCode error : http_errors) {
    function.Run(error);
  }
}

TEST_F(MAYBE_SyncHttpBridgeTest, NetErrorUnreached) {
  // Test deliberately does not start the EmbeddedTestServer.
  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  http_bridge->SetURL(GURL("http://anything:9999"));

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_FALSE(success);
  EXPECT_EQ(-1, response_code);
  EXPECT_NE(0, os_error);
}

TEST_F(MAYBE_SyncHttpBridgeTest, Abort) {
  scoped_refptr<ShuntedHttpBridge> http_bridge(
      new ShuntedHttpBridge(this, true));
  http_bridge->SetURL(GURL("http://www.google.com:9999"));
  http_bridge->SetPostPayload("text/plain", 2, " ");

  int os_error = 0;
  int response_code = 0;

  io_thread()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&HttpBridge::Abort, http_bridge));
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_FALSE(success);
  EXPECT_EQ(net::ERR_ABORTED, os_error);
}

TEST_F(MAYBE_SyncHttpBridgeTest, AbortLate) {
  scoped_refptr<ShuntedHttpBridge> http_bridge(
      new ShuntedHttpBridge(this, false));
  http_bridge->SetURL(GURL("http://www.google.com:9999"));
  http_bridge->SetPostPayload("text/plain", 2, " ");

  int os_error = 0;
  int response_code = 0;

  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  ASSERT_TRUE(success);
  http_bridge->Abort();
  // Ensures no double-free of URLFetcher, etc.
}

// Tests an interesting case where code using the HttpBridge aborts the fetch
// and releases ownership before a pending fetch completed callback is issued by
// the underlying URLFetcher (and before that URLFetcher is destroyed, which
// would cancel the callback).
TEST_F(MAYBE_SyncHttpBridgeTest, AbortAndReleaseBeforeFetchComplete) {
  base::Thread sync_thread("SyncThread");
  sync_thread.Start();

  // First, block the sync thread on the post.
  base::WaitableEvent signal_when_created(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent signal_when_released(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  sync_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&MAYBE_SyncHttpBridgeTest::RunSyncThreadBridgeUseTest,
                     base::Unretained(this), &signal_when_created,
                     &signal_when_released));

  // Stop IO so we can control order of operations.
  base::WaitableEvent io_waiter(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  ASSERT_TRUE(io_thread()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&base::WaitableEvent::Wait,
                                base::Unretained(&io_waiter))));

  signal_when_created.Wait();  // Wait till we have a bridge to abort.
  ASSERT_TRUE(bridge_for_race_test());

  // Schedule the fetch completion callback (but don't run it yet). Take a
  // reference to the bridge (implicitly by binding it to the callback), to
  // simulate what HttpBridge::MakeAsynchronousPost() does.
  ASSERT_TRUE(io_thread()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&syncer::HttpBridge::OnURLLoadComplete,
                                bridge_for_race_test(),
                                std::make_unique<std::string>("success!"))));

  // Abort the fetch. This should be smart enough to handle the case where
  // the bridge is released on the sync therad before the callback scheduled
  // above completes.
  bridge_for_race_test()->Abort();

  // Wait until the sync thread releases its ref on the bridge.
  signal_when_released.Wait();
  ASSERT_FALSE(bridge_for_race_test());

  // Unleash the hounds. The fetch completion callback should fire first, and
  // succeed even though the sync thread already released its ref on the bridge.
  io_waiter.Signal();

  // Done.
  sync_thread.Stop();
  io_thread()->Stop();
}

}  // namespace syncer
