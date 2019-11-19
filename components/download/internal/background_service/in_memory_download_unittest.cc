// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/in_memory_download.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "net/base/io_buffer.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_url_loader_factory.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;

namespace download {
namespace {

const char kTestDownloadData[] =
    "In earlier tellings, the dog had a better reputation than the cat, "
    "however the president veto it.";

MATCHER_P2(InMemoryDownloadMatcher,
           response_headers,
           url_chain,
           "Verify in memory download.") {
  return arg->response_headers()->raw_headers() == response_headers &&
         arg->url_chain() == url_chain;
}

// Dummy callback used for IO_PENDING state in blob operations, this is not
// called when the blob operation is done, but called when chained with other
// IO operations that might return IO_PENDING.
template <typename T>
void SetValue(T* address, T value) {
  *address = value;
}

// Must run on IO thread task runner.
base::WeakPtr<storage::BlobStorageContext> BlobStorageContextGetter(
    storage::BlobStorageContext* blob_context) {
  DCHECK(blob_context);
  return blob_context->AsWeakPtr();
}

class MockDelegate : public InMemoryDownload::Delegate {
 public:
  MockDelegate(BlobContextGetter blob_context_getter)
      : blob_context_getter_(blob_context_getter) {}

  void WaitForCompletion() {
    DCHECK(!run_loop_.running());
    run_loop_.Run();
  }

  // InMemoryDownload::Delegate implementation.
  MOCK_METHOD1(OnDownloadProgress, void(InMemoryDownload*));
  MOCK_METHOD1(OnDownloadStarted, void(InMemoryDownload*));
  void OnDownloadComplete(InMemoryDownload* download) override {
    if (run_loop_.running())
      run_loop_.Quit();
  }
  MOCK_METHOD1(OnUploadProgress, void(InMemoryDownload*));
  void RetrieveBlobContextGetter(
      base::OnceCallback<void(BlobContextGetter)> callback) override {
    std::move(callback).Run(blob_context_getter_);
  }

 private:
  base::RunLoop run_loop_;
  BlobContextGetter blob_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

class InMemoryDownloadTest : public testing::Test {
 public:
  InMemoryDownloadTest() = default;
  ~InMemoryDownloadTest() override = default;

  void SetUp() override {
    io_thread_.reset(new base::Thread("Network and Blob IO thread"));
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    io_thread_->StartWithOptions(options);

    base::RunLoop loop;
    io_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          blob_storage_context_ =
              std::make_unique<storage::BlobStorageContext>();
          loop.Quit();
        }));
    loop.Run();

    auto blob_storage_context_getter = base::BindRepeating(
        &BlobStorageContextGetter, blob_storage_context_.get());
    mock_delegate_ =
        std::make_unique<NiceMock<MockDelegate>>(blob_storage_context_getter);
  }

  void TearDown() override {
    // Say goodbye to |blob_storage_context_| on IO thread.
    io_thread_->task_runner()->DeleteSoon(FROM_HERE,
                                          blob_storage_context_.release());
  }

 protected:
  // Helper method to create a download with request_params.
  void CreateDownload(const RequestParams& request_params) {
    download_ = std::make_unique<InMemoryDownloadImpl>(
        base::GenerateGUID(), request_params, /* request_body= */ nullptr,
        TRAFFIC_ANNOTATION_FOR_TESTS, delegate(), &url_loader_factory_,
        io_thread_->task_runner());
  }

  InMemoryDownload* download() { return download_.get(); }
  MockDelegate* delegate() { return mock_delegate_.get(); }
  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  // Verifies if data read from |blob| is identical as |expected|.
  void VerifyBlobData(const std::string& expected,
                      storage::BlobDataHandle* blob) {
    base::RunLoop run_loop;
    // BlobReader needs to work on IO thread of BlobStorageContext.
    io_thread_->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&InMemoryDownloadTest::VerifyBlobDataOnIO,
                       base::Unretained(this), expected, blob),
        run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  void VerifyBlobDataOnIO(const std::string& expected,
                          storage::BlobDataHandle* blob) {
    DCHECK(blob);
    int bytes_read = 0;
    int async_bytes_read = 0;
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(expected.size());

    auto blob_reader = blob->CreateReader();

    int blob_size = 0;
    blob_reader->CalculateSize(base::BindRepeating(&SetValue<int>, &blob_size));
    EXPECT_EQ(blob_size, 0) << "In memory blob read data synchronously.";
    EXPECT_FALSE(blob->IsBeingBuilt())
        << "InMemoryDownload ensures blob construction completed.";
    storage::BlobReader::Status status = blob_reader->Read(
        buffer.get(), expected.size(), &bytes_read,
        base::BindRepeating(&SetValue<int>, &async_bytes_read));
    EXPECT_EQ(storage::BlobReader::Status::DONE, status);
    EXPECT_EQ(bytes_read, static_cast<int>(expected.size()));
    EXPECT_EQ(async_bytes_read, 0);
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(expected[i], buffer->data()[i]);
    }
  }

  // IO thread used by network and blob IO tasks.
  std::unique_ptr<base::Thread> io_thread_;

  // Created before other objects to provide test environment.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<InMemoryDownloadImpl> download_;
  std::unique_ptr<NiceMock<MockDelegate>> mock_delegate_;

  // Used by SimpleURLLoader network backend.
  network::TestURLLoaderFactory url_loader_factory_;

  // Memory backed blob storage that can never page to disk.
  std::unique_ptr<storage::BlobStorageContext> blob_storage_context_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryDownloadTest);
};

TEST_F(InMemoryDownloadTest, DownloadTest) {
  RequestParams request_params;
  CreateDownload(request_params);
  url_loader_factory()->AddResponse(request_params.url.spec(),
                                    kTestDownloadData);

  EXPECT_CALL(*delegate(), OnDownloadStarted(_));
  // TODO(xingliu): More tests on pause/resume.
  download()->Start();
  delegate()->WaitForCompletion();

  EXPECT_EQ(InMemoryDownload::State::COMPLETE, download()->state());
  auto blob = download()->ResultAsBlob();
  VerifyBlobData(kTestDownloadData, blob.get());
}

TEST_F(InMemoryDownloadTest, RedirectResponseHeaders) {
  RequestParams request_params;
  request_params.url = GURL("https://example.com/firsturl");
  CreateDownload(request_params);

  // Add a redirect.
  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://example.com/redirect12345");
  network::TestURLLoaderFactory::Redirects redirects;
  redirects.push_back({redirect_info, network::mojom::URLResponseHead::New()});

  // Add some random header.
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_head->headers->AddHeader("X-Random-Test-Header: 123");

  // The size must match for download as stream from SimpleUrlLoader.
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = base::size(kTestDownloadData) - 1;

  url_loader_factory()->AddResponse(request_params.url, response_head.Clone(),
                                    kTestDownloadData, status,
                                    std::move(redirects));

  std::vector<GURL> expected_url_chain = {request_params.url,
                                          redirect_info.new_url};

  EXPECT_CALL(*delegate(),
              OnDownloadStarted(InMemoryDownloadMatcher(
                  response_head->headers->raw_headers(), expected_url_chain)));

  download()->Start();
  delegate()->WaitForCompletion();
  EXPECT_EQ(InMemoryDownload::State::COMPLETE, download()->state());

  // Verify the response headers and URL chain. The URL chain should contain
  // the original URL and redirect URL, and should not contain the final URL.
  EXPECT_EQ(download()->url_chain(), expected_url_chain);
  EXPECT_EQ(download()->response_headers()->raw_headers(),
            response_head->headers->raw_headers());

  // Verfiy the data persisted to disk after redirect chain.
  auto blob = download()->ResultAsBlob();
  VerifyBlobData(kTestDownloadData, blob.get());
}

}  // namespace

}  // namespace download
