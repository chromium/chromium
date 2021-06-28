// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/cached_image_fetcher.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"
#include "components/ntp_snippets/remote/remote_suggestions_database.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

using leveldb_proto::test::FakeDB;
using testing::_;
using testing::Eq;
using testing::Property;

namespace ntp_snippets {

namespace {

const char kImageData[] = "data";
const char kImageURL[] = "http://image.test/test.png";
const char kSnippetID[] = "http://localhost";
const ContentSuggestion::ID kSuggestionID(
    Category::FromKnownCategory(KnownCategories::ARTICLES),
    kSnippetID);

enum class TestType {
  kImageCallback,
  kImageDataCallback,
  kBothCallbacks,
};
}  // namespace

// This test is parameterized to run all tests in the three configurations:
// both callbacks used, only image_callback used, only image_data_callback used.
class NtpSnippetsCachedImageFetcherTest
    : public testing::TestWithParam<TestType> {
 public:
  NtpSnippetsCachedImageFetcherTest() {
    RequestThrottler::RegisterProfilePrefs(pref_service_.registry());

    // Setup RemoteSuggestionsDatabase with fake ProtoDBs.
    auto suggestion_db =
        std::make_unique<FakeDB<SnippetProto>>(&suggestion_db_storage_);
    auto image_db =
        std::make_unique<FakeDB<SnippetImageProto>>(&image_db_storage_);
    suggestion_db_ = suggestion_db.get();
    image_db_ = image_db.get();
    database_ = std::make_unique<RemoteSuggestionsDatabase>(
        std::move(suggestion_db), std::move(image_db));
    suggestion_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    image_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    auto decoder = std::make_unique<image_fetcher::FakeImageDecoder>();
    fake_image_decoder_ = decoder.get();
    auto image_fetcher = std::make_unique<image_fetcher::ImageFetcherImpl>(
        std::move(decoder), shared_factory_);

    cached_image_fetcher_ = std::make_unique<ntp_snippets::CachedImageFetcher>(
        std::move(image_fetcher), &pref_service_, database_.get());

    EXPECT_TRUE(database_->IsInitialized());
  }

  ~NtpSnippetsCachedImageFetcherTest() override {
    cached_image_fetcher_.reset();
    database_.reset();
  }

  void Fetch(std::string expected_image_data, bool expect_image) {
    fake_image_decoder()->SetEnabled(GetParam() !=
                                     TestType::kImageDataCallback);
    base::MockCallback<ImageFetchedCallback> image_callback;
    base::MockCallback<ImageDataFetchedCallback> image_data_callback;
    switch (GetParam()) {
      case TestType::kImageCallback: {
        EXPECT_CALL(image_callback,
                    Run(Property(&gfx::Image::IsEmpty, Eq(!expect_image))));
        cached_image_fetcher()->FetchSuggestionImage(
            kSuggestionID, GURL(kImageURL), ImageDataFetchedCallback(),
            image_callback.Get());
        image_db_->GetCallback(true);
      } break;
      case TestType::kImageDataCallback: {
        EXPECT_CALL(image_data_callback, Run(expected_image_data));
        cached_image_fetcher()->FetchSuggestionImage(
            kSuggestionID, GURL(kImageURL), image_data_callback.Get(),
            ImageFetchedCallback());
        image_db_->GetCallback(true);
      } break;
      case TestType::kBothCallbacks: {
        EXPECT_CALL(image_data_callback, Run(expected_image_data));
        EXPECT_CALL(image_callback,
                    Run(Property(&gfx::Image::IsEmpty, Eq(!expect_image))));
        cached_image_fetcher()->FetchSuggestionImage(
            kSuggestionID, GURL(kImageURL), image_data_callback.Get(),
            image_callback.Get());
        image_db_->GetCallback(true);
      } break;
    }
    task_environment_.RunUntilIdle();
  }

  RemoteSuggestionsDatabase* database() { return database_.get(); }
  image_fetcher::FakeImageDecoder* fake_image_decoder() {
    return fake_image_decoder_;
  }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }
  CachedImageFetcher* cached_image_fetcher() {
    return cached_image_fetcher_.get();
  }
  FakeDB<SnippetProto>* suggestion_db() { return suggestion_db_; }
  FakeDB<SnippetImageProto>* image_db() { return image_db_; }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  std::unique_ptr<CachedImageFetcher> cached_image_fetcher_;
  image_fetcher::FakeImageDecoder* fake_image_decoder_;

  std::unique_ptr<RemoteSuggestionsDatabase> database_;
  std::map<std::string, SnippetProto> suggestion_db_storage_;
  std::map<std::string, SnippetImageProto> image_db_storage_;

  // Owned by |database_|.
  FakeDB<SnippetProto>* suggestion_db_;
  FakeDB<SnippetImageProto>* image_db_;

  TestingPrefServiceSimple pref_service_;
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(NtpSnippetsCachedImageFetcherTest);
};

TEST_P(NtpSnippetsCachedImageFetcherTest, FetchImageFromCache) {
  // Save the image in the database.
  database()->SaveImage(kSnippetID, kImageData);
  image_db()->UpdateCallback(true);

  // Do not provide any URL responses and expect that the image is fetched (from
  // cache).
  Fetch(kImageData, true);
}

TEST_P(NtpSnippetsCachedImageFetcherTest, FetchImagePopulatesCache) {
  // Expect the image to be fetched by URL.
  {
    test_url_loader_factory()->AddResponse(kImageURL, kImageData);
    Fetch(kImageData, true);
  }
  // Fetch again. The cache should be populated, no network request is needed.
  {
    test_url_loader_factory()->ClearResponses();
    Fetch(kImageData, true);
  }
}

TEST_P(NtpSnippetsCachedImageFetcherTest, FetchNonExistingImage) {
  const std::string kErrorResponse = "error-response";
  test_url_loader_factory()->AddResponse(kImageURL, kErrorResponse,
                                         net::HTTP_NOT_FOUND);
  // Expect an empty image is fetched if the URL cannot be requested.
  Fetch("", false);
}

INSTANTIATE_TEST_SUITE_P(NTP,
                         NtpSnippetsCachedImageFetcherTest,
                         testing::Values(TestType::kImageCallback,
                                         TestType::kImageDataCallback,
                                         TestType::kBothCallbacks));

}  // namespace ntp_snippets
