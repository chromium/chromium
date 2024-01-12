// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/token.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_background_manager.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/image_fetcher/core/mock_image_decoder.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/wallpaper_search.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/search/ntp_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "skia/ext/image_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

namespace {

using testing::_;
using testing::An;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;

class MockClient
    : public side_panel::customize_chrome::mojom::WallpaperSearchClient {
 public:
  MockClient() = default;
  ~MockClient() override = default;

  mojo::PendingRemote<
      side_panel::customize_chrome::mojom::WallpaperSearchClient>
  BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD1(
      SetHistory,
      void(std::vector<
           side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>));

  mojo::Receiver<side_panel::customize_chrome::mojom::WallpaperSearchClient>
      receiver_{this};
};

class MockWallpaperSearchBackgroundManager
    : public WallpaperSearchBackgroundManager {
 public:
  explicit MockWallpaperSearchBackgroundManager(Profile* profile)
      : WallpaperSearchBackgroundManager(profile) {}
  MOCK_METHOD0(GetHistory, std::vector<HistoryEntry>());
  MOCK_METHOD3(SelectHistoryImage,
               void(const base::Token&,
                    const gfx::Image&,
                    base::ElapsedTimer timer));
  MOCK_METHOD3(SelectLocalBackgroundImage,
               void(const base::Token&,
                    const SkBitmap&,
                    base::ElapsedTimer timer));
  MOCK_METHOD1(SaveCurrentBackgroundToHistory,
               absl::optional<base::Token>(const HistoryEntry& history_entry));
};

std::unique_ptr<TestingProfile> MakeTestingProfile(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TestingPrefServiceSimple* local_state) {
  MockOptimizationGuideKeyedService::Initialize(local_state);
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      OptimizationGuideKeyedServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            testing::NiceMock<MockOptimizationGuideKeyedService>>(context);
      }));
  profile_builder.SetSharedURLLoaderFactory(url_loader_factory);
  auto profile = profile_builder.Build();
  return profile;
}

std::unique_ptr<optimization_guide::ModelQualityLogEntry> ModelQuality() {
  return std::make_unique<optimization_guide::ModelQualityLogEntry>(
      std::make_unique<optimization_guide::proto::LogAiDataRequest>());
}

}  // namespace

class WallpaperSearchHandlerTest : public testing::Test {
 public:
  WallpaperSearchHandlerTest()
      : profile_(MakeTestingProfile(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            &local_state_)),
        mock_optimization_guide_keyed_service_(
            static_cast<MockOptimizationGuideKeyedService*>(
                OptimizationGuideKeyedServiceFactory::GetForProfile(
                    profile_.get()))),
        mock_wallpaper_search_background_manager_(
            MockWallpaperSearchBackgroundManager(profile_.get())) {}

  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kCustomizeChromeWallpaperSearch,
                              optimization_guide::features::
                                  kOptimizationGuideModelExecution},
        /*disabled_features=*/{});
  }

  void TearDown() override {
    MockOptimizationGuideKeyedService::TearDown();
    test_url_loader_factory_.ClearResponses();
  }

  const std::string kGstaticBaseURL =
      "https://www.gstatic.com/chrome-wallpaper-search/";
  const std::string kDescriptorsLoadURL =
      base::StrCat({kGstaticBaseURL, "descriptors_en-US.json"});
  void SetUpDescriptorsResponseWithData(const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {}));
    test_url_loader_factory_.AddResponse(kDescriptorsLoadURL, response);
  }
  void SetUpDescriptorsResponseWithNetworkError() {
    test_url_loader_factory_.AddResponse(kDescriptorsLoadURL, std::string(),
                                         net::HTTP_NOT_FOUND);
  }

  const std::string kInspirationsLoadURL =
      base::StrCat({kGstaticBaseURL, "inspirations.json"});
  void SetUpInspirationsResponseWithData(const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {}));
    test_url_loader_factory_.AddResponse(kInspirationsLoadURL, response);
  }
  void SetUpInspirationsResponseWithNetworkError() {
    test_url_loader_factory_.AddResponse(kInspirationsLoadURL, std::string(),
                                         net::HTTP_NOT_FOUND);
  }

  std::unique_ptr<WallpaperSearchHandler> MakeHandler(int64_t session_id) {
    auto handler = std::make_unique<WallpaperSearchHandler>(
        mojo::PendingReceiver<
            side_panel::customize_chrome::mojom::WallpaperSearchHandler>(),
        mock_client_.BindAndGetRemote(), profile_.get(), &mock_image_decoder_,
        &mock_wallpaper_search_background_manager_, session_id);
    mock_client_.FlushForTesting();
    return handler;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  MockClient& mock_client() { return mock_client_; }
  MockOptimizationGuideKeyedService& mock_optimization_guide_keyed_service() {
    return *mock_optimization_guide_keyed_service_;
  }
  MockWallpaperSearchBackgroundManager&
  mock_wallpaper_search_background_manager() {
    return mock_wallpaper_search_background_manager_;
  }
  image_fetcher::MockImageDecoder& mock_image_decoder() {
    return mock_image_decoder_;
  }
  TestingProfile& profile() { return *profile_.get(); }

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
  image_fetcher::MockImageDecoder mock_image_decoder_;
  testing::NiceMock<MockClient> mock_client_;
  base::HistogramTester histogram_tester_;
  MockWallpaperSearchBackgroundManager
      mock_wallpaper_search_background_manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(WallpaperSearchHandlerTest, GetHistory) {
  base::OnceCallback<void(const gfx::Image&)> decoder_callback;
  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      history_images;
  EXPECT_CALL(mock_wallpaper_search_background_manager(), GetHistory());
  EXPECT_CALL(mock_client(), SetHistory(_))
      .WillOnce(MoveArg<0>(&history_images));
  EXPECT_CALL(mock_image_decoder(), DecodeImage(_, _, _, _))
      .WillOnce(Invoke(
          [&decoder_callback](const std::string& image_data,
                              const gfx::Size& desired_image_frame_size,
                              data_decoder::DataDecoder* data_decoder,
                              image_fetcher::ImageDecodedCallback callback) {
            decoder_callback = std::move(callback);
          }));

  auto handler = MakeHandler(/*session_id=*/123);

  // Create test bitmap.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(64, 32);
  bitmap.eraseColor(SK_ColorRED);
  std::vector<unsigned char> encoded;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false,
                                    &encoded);

  // Write bitmap to file.
  base::Token token = base::Token::CreateRandom();
  base::WriteFile(profile().GetPath().AppendASCII(
                      token.ToString() +
                      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename),
                  base::as_bytes(base::make_span(
                      std::string(encoded.begin(), encoded.end()))));

  // Return test image from WallpaperSearchBackgroundManager::GetHistory().
  std::vector<HistoryEntry> history;
  HistoryEntry history_entry = HistoryEntry(token);
  history_entry.subject = "foo";
  history_entry.mood = "bar";
  history_entry.style = "foobar";
  history.push_back(history_entry);
  ON_CALL(mock_wallpaper_search_background_manager(), GetHistory())
      .WillByDefault(testing::Return(history));

  handler->UpdateHistory();
  task_environment().RunUntilIdle();

  std::move(decoder_callback).Run(gfx::Image::CreateFrom1xBitmap(bitmap));
  mock_client().FlushForTesting();

  ASSERT_EQ(static_cast<int>(history_images.size()), 1);

  // Check that resized encoded versions of the original bitmaps is what we
  // get back and that the id matches.
  // The bitmap's width should be twice the height to be the same aspect
  // ratio as the original image.
  auto resized_bitmap = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_GOOD, 200, 100);
  std::vector<unsigned char> resized_encoded;
  gfx::PNGCodec::EncodeBGRASkBitmap(
      resized_bitmap, /*discard_transparency=*/false, &resized_encoded);
  EXPECT_EQ(history_images[0]->image, base::Base64Encode(resized_encoded));
  EXPECT_EQ(history_images[0]->id.ToString(), token.ToString());
  EXPECT_EQ(history_images[0]->descriptors->subject, history_entry.subject);
  EXPECT_EQ(history_images[0]->descriptors->mood, history_entry.mood);
  EXPECT_EQ(history_images[0]->descriptors->style, history_entry.style);
}

TEST_F(WallpaperSearchHandlerTest,
       GetDescriptors_Success_DescriptorsFormatCorrect) {
  side_panel::customize_chrome::mojom::DescriptorsPtr descriptors;
  base::MockCallback<WallpaperSearchHandler::GetDescriptorsCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&descriptors](side_panel::customize_chrome::mojom::DescriptorsPtr
                             descriptors_ptr_arg) {
            descriptors = std::move(descriptors_ptr_arg);
          }));
  SetUpDescriptorsResponseWithData(
      R"()]}'
        {
          "descriptor_a":[
            {"category":"foo","labels":["bar","baz"]},
            {"category":"qux","labels":["foobar"]}
          ],
          "descriptor_b":[
            {"label":"foo","image":"bar.png"}
          ],
          "descriptor_c":["foo","bar","baz"]
        })");
  auto handler = MakeHandler(/*session_id=*/123);

  ASSERT_FALSE(descriptors);
  handler->GetDescriptors(callback.Get());
  task_environment().RunUntilIdle();

  EXPECT_TRUE(descriptors);

  const auto& descriptor_a = descriptors->descriptor_a;
  EXPECT_EQ(2u, descriptor_a.size());
  const auto& foo_descriptor = descriptor_a[0];
  EXPECT_EQ(foo_descriptor->category, "foo");
  EXPECT_EQ(2u, foo_descriptor->labels.size());
  EXPECT_EQ("bar", foo_descriptor->labels[0]);
  EXPECT_EQ("baz", foo_descriptor->labels[1]);
  const auto& qux_descriptor = descriptor_a[1];
  EXPECT_EQ(qux_descriptor->category, "qux");
  EXPECT_EQ(1u, qux_descriptor->labels.size());
  EXPECT_EQ("foobar", qux_descriptor->labels[0]);

  const auto& descriptor_b = descriptors->descriptor_b;
  EXPECT_EQ(1u, descriptor_b.size());
  EXPECT_EQ("foo", descriptor_b[0]->label);
  EXPECT_EQ(base::StrCat({kGstaticBaseURL, "bar.png"}),
            descriptor_b[0]->image_path);

  const auto& descriptor_c = descriptors->descriptor_c;
  EXPECT_EQ(3u, descriptor_c.size());
  EXPECT_EQ("foo", descriptor_c[0]);
  EXPECT_EQ("bar", descriptor_c[1]);
  EXPECT_EQ("baz", descriptor_c[2]);
}

TEST_F(WallpaperSearchHandlerTest,
       GetDescriptors_Success_PrioritizesLatestRequest) {
  side_panel::customize_chrome::mojom::DescriptorsPtr descriptors;
  side_panel::customize_chrome::mojom::DescriptorsPtr descriptors_2;
  base::MockCallback<WallpaperSearchHandler::GetDescriptorsCallback> callback;
  base::MockCallback<WallpaperSearchHandler::GetDescriptorsCallback> callback_2;
  EXPECT_CALL(callback, Run(_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&descriptors](side_panel::customize_chrome::mojom::DescriptorsPtr
                             descriptors_ptr_arg) {
            descriptors = std::move(descriptors_ptr_arg);
          }));
  EXPECT_CALL(callback_2, Run(_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&descriptors_2](side_panel::customize_chrome::mojom::DescriptorsPtr
                               descriptors_2_ptr_arg) {
            descriptors_2 = std::move(descriptors_2_ptr_arg);
          }));
  SetUpDescriptorsResponseWithData(
      R"()]}'
        {
          "descriptor_a":[
            {"category":"foo","labels":["bar"]}
          ],
          "descriptor_b":[
            {"label":"foo","image":"bar.png"}
          ],
          "descriptor_c":["foo"]
        })");
  auto handler = MakeHandler(/*session_id=*/123);

  handler->GetDescriptors(callback.Get());
  handler->GetDescriptors(callback_2.Get());
  task_environment().RunUntilIdle();

  EXPECT_FALSE(descriptors);
  EXPECT_TRUE(descriptors_2);
  const auto& descriptor_a = descriptors_2->descriptor_a;
  EXPECT_EQ(1u, descriptor_a.size());
  const auto& foo_descriptor = descriptor_a[0];
  EXPECT_EQ(foo_descriptor->category, "foo");
  EXPECT_EQ(1u, foo_descriptor->labels.size());
  EXPECT_EQ("bar", foo_descriptor->labels[0]);
  const auto& descriptor_b = descriptors_2->descriptor_b;
  EXPECT_EQ(1u, descriptor_b.size());
  EXPECT_EQ("foo", descriptor_b[0]->label);
  EXPECT_EQ(base::StrCat({kGstaticBaseURL, "bar.png"}),
            descriptor_b[0]->image_path);
  const auto& descriptor_c = descriptors_2->descriptor_c;
  EXPECT_EQ(1u, descriptor_c.size());
  EXPECT_EQ("foo", descriptor_c[0]);
}

TEST_F(WallpaperSearchHandlerTest,
       GetDescriptors_Failure_DescriptorsFormatIncorrect) {
  side_panel::customize_chrome::mojom::DescriptorsPtr descriptors;
  base::MockCallback<WallpaperSearchHandler::GetDescriptorsCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&descriptors](side_panel::customize_chrome::mojom::DescriptorsPtr
                             descriptors_ptr_arg) {
            descriptors = std::move(descriptors_ptr_arg);
          }));
  SetUpDescriptorsResponseWithData(
      R"()]}'
        {"descriptor_a":[
          {"category":"foo"}
      ]})");
  ASSERT_FALSE(descriptors);
  auto handler = MakeHandler(/*session_id=*/123);

  handler->GetDescriptors(callback.Get());
  task_environment().RunUntilIdle();

  EXPECT_FALSE(descriptors);
}

TEST_F(WallpaperSearchHandlerTest, GetDescriptors_Failure_NoValidDescriptors) {
  side_panel::customize_chrome::mojom::DescriptorsPtr descriptors;
  base::MockCallback<WallpaperSearchHandler::GetDescriptorsCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&descriptors](side_panel::customize_chrome::mojom::DescriptorsPtr
                             descriptors_ptr_arg) {
            descriptors = std::move(descriptors_ptr_arg);
          }));
  SetUpDescriptorsResponseWithData(
      R"()]}'
        {"not_a_valid_descriptor":[
          {"category":"foo","labels":["bar","baz"]},
          {"category":"qux","labels":["foobar"]}
      ]})");
  ASSERT_FALSE(descriptors);
  auto handler = MakeHandler(/*session_id=*/123);

  handler->GetDescriptors(callback.Get());
  task_environment().RunUntilIdle();

  EXPECT_FALSE(descriptors);
}

TEST_F(WallpaperSearchHandlerTest, GetDescriptors_Failure_DataIsUnreachable) {
  side_panel::customize_chrome::mojom::DescriptorsPtr descriptors;
  base::MockCallback<WallpaperSearchHandler::GetDescriptorsCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&descriptors](side_panel::customize_chrome::mojom::DescriptorsPtr
                             descriptors_ptr_arg) {
            descriptors = std::move(descriptors_ptr_arg);
          }));
  SetUpDescriptorsResponseWithNetworkError();
  ASSERT_FALSE(descriptors);
  auto handler = MakeHandler(/*session_id=*/123);

  handler->GetDescriptors(callback.Get());
  task_environment().RunUntilIdle();

  EXPECT_FALSE(descriptors);
}

TEST_F(WallpaperSearchHandlerTest, GetWallpaperSearchResults_Success) {
  optimization_guide::proto::WallpaperSearchRequest request;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      done_callback;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback1;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback2;
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _))
      .WillOnce(Invoke(
          [&request, &done_callback](
              optimization_guide::proto::ModelExecutionFeature feature_arg,
              const google::protobuf::MessageLite& request_arg,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  done_callback_arg) {
            ASSERT_EQ(request.GetTypeName(), request_arg.GetTypeName());
            request.CheckTypeAndMergeFrom(request_arg);
            done_callback = std::move(done_callback_arg);
          }));
  EXPECT_CALL(mock_image_decoder(), DecodeImage(_, _, _, _))
      .Times(2)
      .WillOnce(Invoke(
          [&decoder_callback1](const std::string& image_data,
                               const gfx::Size& desired_image_frame_size,
                               data_decoder::DataDecoder* data_decoder,
                               image_fetcher::ImageDecodedCallback callback) {
            decoder_callback1 = std::move(callback);
          }))
      .WillOnce(Invoke(
          [&decoder_callback2](const std::string& image_data,
                               const gfx::Size& desired_image_frame_size,
                               data_decoder::DataDecoder* data_decoder,
                               image_fetcher::ImageDecodedCallback callback) {
            decoder_callback2 = std::move(callback);
          }));
  base::MockCallback<WallpaperSearchHandler::GetWallpaperSearchResultsCallback>
      callback;
  auto handler = MakeHandler(/*session_id=*/123);

  side_panel::customize_chrome::mojom::ResultDescriptorsPtr result_descriptors =
      side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors->subject = "foo";
  result_descriptors->style = "bar";
  result_descriptors->mood = "baz";
  result_descriptors->color =
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorWHITE);
  handler->GetWallpaperSearchResults(std::move(result_descriptors),
                                     callback.Get());
  EXPECT_EQ("foo", request.descriptors().descriptor_a());
  EXPECT_EQ("bar", request.descriptors().descriptor_b());
  EXPECT_EQ("baz", request.descriptors().descriptor_c());
  EXPECT_EQ("#FFFFFF", request.descriptors().descriptor_d());

  optimization_guide::proto::WallpaperSearchResponse response;

  // Create test bitmap 1 and add it to response.
  SkBitmap bitmap1;
  bitmap1.allocN32Pixels(64, 32);
  bitmap1.eraseColor(SK_ColorRED);
  std::vector<unsigned char> encoded1;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap1, /*discard_transparency=*/false,
                                    &encoded1);
  auto* image1 = response.add_images();
  image1->set_encoded_image(std::string(encoded1.begin(), encoded1.end()));
  image1->set_image_id(111);

  // Create test bitmap 2 and add it to response.
  SkBitmap bitmap2;
  bitmap2.allocN32Pixels(32, 32);
  bitmap2.eraseColor(SK_ColorBLUE);
  std::vector<unsigned char> encoded2;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap2, /*discard_transparency=*/false,
                                    &encoded2);
  auto* image2 = response.add_images();
  image2->set_encoded_image(std::string(encoded2.begin(), encoded2.end()));
  image2->set_image_id(222);

  // Serialize and set result to later send to done_callback.
  std::string serialized_metadata;
  response.SerializeToString(&serialized_metadata);
  optimization_guide::proto::Any result;
  result.set_value(serialized_metadata);
  result.set_type_url("type.googleapis.com/" + response.GetTypeName());

  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      images;
  side_panel::customize_chrome::mojom::WallpaperSearchStatus status;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), MoveArg<1>(&images)));

  // Advance clock to test request latency.
  task_environment().AdvanceClock(base::Milliseconds(321));

  std::move(done_callback).Run(base::ok(result), ModelQuality());

  // Advance clock to test processing latency.
  task_environment().AdvanceClock(base::Milliseconds(345));
  std::move(decoder_callback1).Run(gfx::Image::CreateFrom1xBitmap(bitmap1));
  std::move(decoder_callback2).Run(gfx::Image::CreateFrom1xBitmap(bitmap2));

  ASSERT_EQ(status,
            side_panel::customize_chrome::mojom::WallpaperSearchStatus::kOk);
  ASSERT_EQ(static_cast<int>(images.size()), response.images_size());

  // Check that resized encoded versions of the original bitmaps is what we
  // get back.
  // The first bitmap's width should be twice the height to be the same aspect
  // ratio as the original image.
  auto resized_bitmap1 = skia::ImageOperations::Resize(
      bitmap1, skia::ImageOperations::RESIZE_GOOD, 200, 100);
  std::vector<unsigned char> resized_encoded1;
  gfx::PNGCodec::EncodeBGRASkBitmap(
      resized_bitmap1, /*discard_transparency=*/false, &resized_encoded1);
  EXPECT_EQ(images[0]->image, base::Base64Encode(resized_encoded1));

  auto resized_bitmap2 = skia::ImageOperations::Resize(
      bitmap2, skia::ImageOperations::RESIZE_GOOD, 100, 100);
  std::vector<unsigned char> resized_encoded2;
  gfx::PNGCodec::EncodeBGRASkBitmap(
      resized_bitmap2, /*discard_transparency=*/false, &resized_encoded2);
  EXPECT_EQ(images[1]->image, base::Base64Encode(resized_encoded2));
  histogram_tester().ExpectBucketCount(
      "NewTabPage.WallpaperSearch.GetResultProcessingLatency", 345, 1);

  // Expect upload is called once during destruction.
  std::vector<
      std::unique_ptr<optimization_guide::proto::WallpaperSearchQuality>>
      qualities;
  ON_CALL(mock_optimization_guide_keyed_service(), UploadModelQualityLogs(_))
      .WillByDefault(Invoke(
          [&qualities](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                           log_entry) {
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_wallpaper_search()
                            ->has_quality_data());
            // Images should be cleared for logging.
            EXPECT_EQ(log_entry->log_ai_data_request()
                          ->mutable_wallpaper_search()
                          ->mutable_response_data()
                          ->images_size(),
                      0);

            qualities.push_back(
                std::make_unique<
                    optimization_guide::proto::WallpaperSearchQuality>(
                    log_entry->log_ai_data_request()
                        ->mutable_wallpaper_search()
                        ->quality_data()));
          }));

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(1u, qualities.size());
  EXPECT_EQ(123, qualities[0]->session_id());
  EXPECT_EQ(0, qualities[0]->index());
  EXPECT_TRUE(qualities[0]->final_request_in_session());
  EXPECT_EQ(321, qualities[0]->request_latency_ms());
  ASSERT_EQ(2, qualities[0]->images_quality_size());
  EXPECT_EQ(111, qualities[0]->images_quality(0).image_id());
  EXPECT_FALSE(qualities[0]->images_quality(0).previewed());
  EXPECT_FALSE(qualities[0]->images_quality(0).selected());
  EXPECT_EQ(222, qualities[0]->images_quality(1).image_id());
  EXPECT_FALSE(qualities[0]->images_quality(1).previewed());
  EXPECT_FALSE(qualities[0]->images_quality(1).selected());
}

TEST_F(WallpaperSearchHandlerTest, GetWallpaperSearchResults_MultipleRequests) {
  // FIRST REQUEST.
  optimization_guide::proto::WallpaperSearchRequest request1;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      done_callback1;
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _))
      .WillOnce(Invoke(
          [&request1, &done_callback1](
              optimization_guide::proto::ModelExecutionFeature feature_arg,
              const google::protobuf::MessageLite& request_arg,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  done_callback_arg) {
            ASSERT_EQ(request1.GetTypeName(), request_arg.GetTypeName());
            request1.CheckTypeAndMergeFrom(request_arg);
            done_callback1 = std::move(done_callback_arg);
          }));
  base::MockCallback<WallpaperSearchHandler::GetWallpaperSearchResultsCallback>
      callback1;
  auto handler = MakeHandler(/*session_id=*/123);

  side_panel::customize_chrome::mojom::ResultDescriptorsPtr result_descriptors =
      side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors->subject = "foo1";
  result_descriptors->style = "bar1";
  result_descriptors->mood = "baz1";
  result_descriptors->color =
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorWHITE);
  handler->GetWallpaperSearchResults(std::move(result_descriptors),
                                     callback1.Get());
  EXPECT_EQ("foo1", request1.descriptors().descriptor_a());
  EXPECT_EQ("bar1", request1.descriptors().descriptor_b());
  EXPECT_EQ("baz1", request1.descriptors().descriptor_c());
  EXPECT_EQ("#FFFFFF", request1.descriptors().descriptor_d());

  // Serialize and set result to later send to done_callback.
  optimization_guide::proto::WallpaperSearchResponse response1;
  std::string serialized_metadata1;
  response1.SerializeToString(&serialized_metadata1);
  optimization_guide::proto::Any result1;
  result1.set_value(serialized_metadata1);
  result1.set_type_url("type.googleapis.com/" + response1.GetTypeName());

  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      images1;
  side_panel::customize_chrome::mojom::WallpaperSearchStatus status1;
  EXPECT_CALL(callback1, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status1), MoveArg<1>(&images1)));

  // Advance clock to test request latency.
  task_environment().AdvanceClock(base::Milliseconds(321));

  std::move(done_callback1).Run(base::ok(result1), ModelQuality());

  ASSERT_EQ(status1,
            side_panel::customize_chrome::mojom::WallpaperSearchStatus::kError);
  ASSERT_EQ(static_cast<int>(images1.size()), response1.images_size());

  // Simulate that front-end has received the images and rendered them.
  handler->SetResultRenderTime(
      {}, base::Time::Now().InMillisecondsFSinceUnixEpoch());

  // SECOND REQUEST.
  optimization_guide::proto::WallpaperSearchRequest request2;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      done_callback2;
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _))
      .WillOnce(Invoke(
          [&request2, &done_callback2](
              optimization_guide::proto::ModelExecutionFeature feature_arg,
              const google::protobuf::MessageLite& request_arg,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  done_callback_arg) {
            ASSERT_EQ(request2.GetTypeName(), request_arg.GetTypeName());
            request2.CheckTypeAndMergeFrom(request_arg);
            done_callback2 = std::move(done_callback_arg);
          }));
  base::MockCallback<WallpaperSearchHandler::GetWallpaperSearchResultsCallback>
      callback2;

  side_panel::customize_chrome::mojom::ResultDescriptorsPtr
      result_descriptors2 =
          side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors2->subject = "foo2";
  result_descriptors2->style = "bar2";
  result_descriptors2->mood = "baz2";
  result_descriptors2->color =
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorRED);
  handler->GetWallpaperSearchResults(std::move(result_descriptors2),
                                     callback2.Get());
  EXPECT_EQ("foo2", request2.descriptors().descriptor_a());
  EXPECT_EQ("bar2", request2.descriptors().descriptor_b());
  EXPECT_EQ("baz2", request2.descriptors().descriptor_c());
  EXPECT_EQ("#FF0000", request2.descriptors().descriptor_d());

  optimization_guide::proto::WallpaperSearchResponse response2;
  std::string serialized_metadata2;
  response2.SerializeToString(&serialized_metadata2);
  optimization_guide::proto::Any result2;
  result2.set_value(serialized_metadata2);
  result2.set_type_url("type.googleapis.com/" + response2.GetTypeName());

  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      images2;
  side_panel::customize_chrome::mojom::WallpaperSearchStatus status2;
  EXPECT_CALL(callback2, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status2), MoveArg<1>(&images2)));

  // Advance clock to test request latency.
  task_environment().AdvanceClock(base::Milliseconds(456));

  std::move(done_callback2).Run(base::ok(result2), ModelQuality());

  ASSERT_EQ(status2,
            side_panel::customize_chrome::mojom::WallpaperSearchStatus::kError);
  ASSERT_EQ(static_cast<int>(images2.size()), response2.images_size());

  // We upload two log entries corresponding to the two requests in a
  // session.
  // Expect upload is called once during destruction.
  std::vector<
      std::unique_ptr<optimization_guide::proto::WallpaperSearchQuality>>
      qualities;
  ON_CALL(mock_optimization_guide_keyed_service(), UploadModelQualityLogs(_))
      .WillByDefault(Invoke(
          [&qualities](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                           log_entry) {
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_wallpaper_search()
                            ->has_quality_data());
            qualities.push_back(
                std::make_unique<
                    optimization_guide::proto::WallpaperSearchQuality>(
                    log_entry->log_ai_data_request()
                        ->mutable_wallpaper_search()
                        ->quality_data()));
          }));

  // Simulate that front-end has received the images and rendered them.
  handler->SetResultRenderTime(
      {}, base::Time::Now().InMillisecondsFSinceUnixEpoch());

  // Advance clock to test complete latency.
  task_environment().AdvanceClock(base::Milliseconds(567));

  // Quality logs on destruction and when a second request is made.
  handler.reset();
  EXPECT_EQ(2u, qualities.size());
  // First request.
  EXPECT_EQ(123, qualities[0]->session_id());
  EXPECT_EQ(0, qualities[0]->index());
  EXPECT_FALSE(qualities[0]->final_request_in_session());
  EXPECT_EQ(321, qualities[0]->request_latency_ms());
  EXPECT_EQ(456, qualities[0]->complete_latency_ms());
  EXPECT_EQ(0, qualities[0]->images_quality_size());
  // Second request.
  EXPECT_EQ(123, qualities[1]->session_id());
  EXPECT_EQ(1, qualities[1]->index());
  EXPECT_TRUE(qualities[1]->final_request_in_session());
  EXPECT_EQ(456, qualities[1]->request_latency_ms());
  EXPECT_EQ(567, qualities[1]->complete_latency_ms());
  EXPECT_EQ(0, qualities[1]->images_quality_size());
}

TEST_F(WallpaperSearchHandlerTest,
       GetWallpaperSearchResults_TwoDescriptorsQueryFormatCorrect) {
  optimization_guide::proto::WallpaperSearchRequest request;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback1;
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _))
      .WillOnce(Invoke(
          [&request](
              optimization_guide::proto::ModelExecutionFeature feature_arg,
              const google::protobuf::MessageLite& request_arg,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  done_callback_arg) {
            ASSERT_EQ(request.GetTypeName(), request_arg.GetTypeName());
            request.CheckTypeAndMergeFrom(request_arg);
          }));

  testing::NiceMock<base::MockCallback<
      WallpaperSearchHandler::GetWallpaperSearchResultsCallback>>
      callback;
  auto handler = MakeHandler(/*session_id=*/123);

  side_panel::customize_chrome::mojom::ResultDescriptorsPtr result_descriptors =
      side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors->subject = "foo";
  result_descriptors->color =
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorRED);
  handler->GetWallpaperSearchResults(std::move(result_descriptors),
                                     callback.Get());

  EXPECT_EQ("foo", request.descriptors().descriptor_a());
  EXPECT_TRUE(request.descriptors().descriptor_b().empty());
  EXPECT_TRUE(request.descriptors().descriptor_c().empty());
  EXPECT_EQ("#FF0000", request.descriptors().descriptor_d());
}

TEST_F(WallpaperSearchHandlerTest, GetWallpaperSearchResults_ConvertsHueToHex) {
  optimization_guide::proto::WallpaperSearchRequest request;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback1;
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _))
      .WillOnce(Invoke(
          [&request](
              optimization_guide::proto::ModelExecutionFeature feature_arg,
              const google::protobuf::MessageLite& request_arg,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  done_callback_arg) {
            ASSERT_EQ(request.GetTypeName(), request_arg.GetTypeName());
            request.CheckTypeAndMergeFrom(request_arg);
          }));

  testing::NiceMock<base::MockCallback<
      WallpaperSearchHandler::GetWallpaperSearchResultsCallback>>
      callback;
  auto handler = MakeHandler(/*session_id=*/123);

  side_panel::customize_chrome::mojom::ResultDescriptorsPtr result_descriptors =
      side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors->subject = "foo";
  result_descriptors->color =
      side_panel::customize_chrome::mojom::DescriptorDValue::NewHue(0);
  handler->GetWallpaperSearchResults(std::move(result_descriptors),
                                     callback.Get());

  EXPECT_EQ("foo", request.descriptors().descriptor_a());
  EXPECT_TRUE(request.descriptors().descriptor_b().empty());
  EXPECT_TRUE(request.descriptors().descriptor_c().empty());
  EXPECT_EQ("#FF0000", request.descriptors().descriptor_d());
}

TEST_F(WallpaperSearchHandlerTest, GetWallpaperSearchResults_NoResponse) {
  optimization_guide::proto::WallpaperSearchRequest request;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      done_callback;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback1;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback2;
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _))
      .WillOnce(Invoke(
          [&request, &done_callback](
              optimization_guide::proto::ModelExecutionFeature feature_arg,
              const google::protobuf::MessageLite& request_arg,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  done_callback_arg) {
            ASSERT_EQ(request.GetTypeName(), request_arg.GetTypeName());
            request.CheckTypeAndMergeFrom(request_arg);
            done_callback = std::move(done_callback_arg);
          }));
  base::MockCallback<WallpaperSearchHandler::GetWallpaperSearchResultsCallback>
      callback;
  auto handler = MakeHandler(/*session_id=*/123);

  side_panel::customize_chrome::mojom::ResultDescriptorsPtr result_descriptors =
      side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors->subject = "foo";
  handler->GetWallpaperSearchResults(std::move(result_descriptors),
                                     callback.Get());
  EXPECT_EQ("foo", request.descriptors().descriptor_a());
  EXPECT_TRUE(request.descriptors().descriptor_b().empty());
  EXPECT_TRUE(request.descriptors().descriptor_c().empty());
  EXPECT_TRUE(request.descriptors().descriptor_d().empty());

  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      images;
  side_panel::customize_chrome::mojom::WallpaperSearchStatus status;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), MoveArg<1>(&images)));

  // Advance clock to test request latency.
  task_environment().AdvanceClock(base::Milliseconds(321));

  std::move(done_callback)
      .Run(
          base::unexpected(
              optimization_guide::OptimizationGuideModelExecutionError::
                  FromModelExecutionError(
                      optimization_guide::OptimizationGuideModelExecutionError::
                          ModelExecutionError::kGenericFailure)),
          ModelQuality());

  EXPECT_EQ(status,
            side_panel::customize_chrome::mojom::WallpaperSearchStatus::kError);
  EXPECT_EQ(images.size(), 0u);

  std::vector<
      std::unique_ptr<optimization_guide::proto::WallpaperSearchQuality>>
      qualities;
  ON_CALL(mock_optimization_guide_keyed_service(), UploadModelQualityLogs(_))
      .WillByDefault(Invoke(
          [&qualities](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                           log_entry) {
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_wallpaper_search()
                            ->has_quality_data());
            qualities.push_back(
                std::make_unique<
                    optimization_guide::proto::WallpaperSearchQuality>(
                    log_entry->log_ai_data_request()
                        ->mutable_wallpaper_search()
                        ->quality_data()));
          }));

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(123, qualities[0]->session_id());
  EXPECT_EQ(0, qualities[0]->index());
  EXPECT_TRUE(qualities[0]->final_request_in_session());
  EXPECT_EQ(321, qualities[0]->request_latency_ms());
  EXPECT_EQ(0, qualities[0]->images_quality_size());
}

TEST_F(WallpaperSearchHandlerTest, GetWallpaperSearchResults_NoImages) {
  optimization_guide::proto::WallpaperSearchRequest request;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      done_callback;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback1;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback2;
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _))
      .WillOnce(Invoke(
          [&request, &done_callback](
              optimization_guide::proto::ModelExecutionFeature feature_arg,
              const google::protobuf::MessageLite& request_arg,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  done_callback_arg) {
            ASSERT_EQ(request.GetTypeName(), request_arg.GetTypeName());
            request.CheckTypeAndMergeFrom(request_arg);
            done_callback = std::move(done_callback_arg);
          }));
  base::MockCallback<WallpaperSearchHandler::GetWallpaperSearchResultsCallback>
      callback;
  auto handler = MakeHandler(/*session_id=*/123);

  side_panel::customize_chrome::mojom::ResultDescriptorsPtr result_descriptors =
      side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors->subject = "foo";
  handler->GetWallpaperSearchResults(std::move(result_descriptors),
                                     callback.Get());
  EXPECT_EQ("foo", request.descriptors().descriptor_a());
  EXPECT_TRUE(request.descriptors().descriptor_b().empty());
  EXPECT_TRUE(request.descriptors().descriptor_c().empty());
  EXPECT_TRUE(request.descriptors().descriptor_d().empty());

  optimization_guide::proto::WallpaperSearchResponse response;
  std::string serialized_metadata;
  response.SerializeToString(&serialized_metadata);
  optimization_guide::proto::Any result;
  result.set_value(serialized_metadata);
  result.set_type_url("type.googleapis.com/" + response.GetTypeName());

  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      images;
  side_panel::customize_chrome::mojom::WallpaperSearchStatus status;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), MoveArg<1>(&images)));

  // Advance clock to test request latency.
  task_environment().AdvanceClock(base::Milliseconds(321));

  std::move(done_callback).Run(base::ok(result), ModelQuality());

  std::vector<
      std::unique_ptr<optimization_guide::proto::WallpaperSearchQuality>>
      qualities;
  ON_CALL(mock_optimization_guide_keyed_service(), UploadModelQualityLogs(_))
      .WillByDefault(Invoke(
          [&qualities](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                           log_entry) {
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_wallpaper_search()
                            ->has_quality_data());
            qualities.push_back(
                std::make_unique<
                    optimization_guide::proto::WallpaperSearchQuality>(
                    log_entry->log_ai_data_request()
                        ->mutable_wallpaper_search()
                        ->quality_data()));
          }));

  EXPECT_EQ(status,
            side_panel::customize_chrome::mojom::WallpaperSearchStatus::kError);
  EXPECT_EQ(static_cast<int>(images.size()), response.images_size());

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(123, qualities[0]->session_id());
  EXPECT_EQ(0, qualities[0]->index());
  EXPECT_TRUE(qualities[0]->final_request_in_session());
  EXPECT_EQ(321, qualities[0]->request_latency_ms());
  EXPECT_EQ(0, qualities[0]->images_quality_size());
}

TEST_F(WallpaperSearchHandlerTest, GetWallpaperSearchResults_RequestThrottled) {
  optimization_guide::proto::WallpaperSearchRequest request;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      done_callback;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback1;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback2;
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _))
      .WillOnce(Invoke(
          [&request, &done_callback](
              optimization_guide::proto::ModelExecutionFeature feature_arg,
              const google::protobuf::MessageLite& request_arg,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  done_callback_arg) {
            ASSERT_EQ(request.GetTypeName(), request_arg.GetTypeName());
            request.CheckTypeAndMergeFrom(request_arg);
            done_callback = std::move(done_callback_arg);
          }));
  base::MockCallback<WallpaperSearchHandler::GetWallpaperSearchResultsCallback>
      callback;
  auto handler = MakeHandler(/*session_id=*/123);

  side_panel::customize_chrome::mojom::ResultDescriptorsPtr result_descriptors =
      side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors->subject = "foo";
  handler->GetWallpaperSearchResults(std::move(result_descriptors),
                                     callback.Get());
  EXPECT_EQ("foo", request.descriptors().descriptor_a());
  EXPECT_TRUE(request.descriptors().descriptor_b().empty());
  EXPECT_TRUE(request.descriptors().descriptor_c().empty());
  EXPECT_TRUE(request.descriptors().descriptor_d().empty());

  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      images;
  side_panel::customize_chrome::mojom::WallpaperSearchStatus status;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), MoveArg<1>(&images)));

  // Advance clock to test request latency.
  task_environment().AdvanceClock(base::Milliseconds(321));

  std::move(done_callback)
      .Run(
          base::unexpected(
              optimization_guide::OptimizationGuideModelExecutionError::
                  FromModelExecutionError(
                      optimization_guide::OptimizationGuideModelExecutionError::
                          ModelExecutionError::kRequestThrottled)),
          ModelQuality());

  EXPECT_EQ(status, side_panel::customize_chrome::mojom::WallpaperSearchStatus::
                        kRequestThrottled);
  EXPECT_EQ(images.size(), 0u);

  std::vector<
      std::unique_ptr<optimization_guide::proto::WallpaperSearchQuality>>
      qualities;
  ON_CALL(mock_optimization_guide_keyed_service(), UploadModelQualityLogs(_))
      .WillByDefault(Invoke(
          [&qualities](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                           log_entry) {
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_wallpaper_search()
                            ->has_quality_data());
            qualities.push_back(
                std::make_unique<
                    optimization_guide::proto::WallpaperSearchQuality>(
                    log_entry->log_ai_data_request()
                        ->mutable_wallpaper_search()
                        ->quality_data()));
          }));

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(123, qualities[0]->session_id());
  EXPECT_EQ(0, qualities[0]->index());
  EXPECT_TRUE(qualities[0]->final_request_in_session());
  EXPECT_EQ(321, qualities[0]->request_latency_ms());
  EXPECT_EQ(0, qualities[0]->images_quality_size());
}

TEST_F(WallpaperSearchHandlerTest, SetBackgroundToHistoryImage) {
  base::OnceCallback<void(const gfx::Image&)> decoder_callback;
  base::Token token_arg;
  gfx::Image image_arg;
  base::ElapsedTimer timer;
  HistoryEntry history_entry_arg;
  EXPECT_CALL(mock_wallpaper_search_background_manager(),
              SelectHistoryImage(_, _, _))
      .WillOnce(DoAll(MoveArg<0>(&token_arg), MoveArg<1>(&image_arg),
                      MoveArg<2>(&timer)));
  EXPECT_CALL(mock_image_decoder(), DecodeImage(_, _, _, _))
      .WillOnce(Invoke(
          [&decoder_callback](const std::string& image_data,
                              const gfx::Size& desired_image_frame_size,
                              data_decoder::DataDecoder* data_decoder,
                              image_fetcher::ImageDecodedCallback callback) {
            decoder_callback = std::move(callback);
          }));

  auto handler = MakeHandler(/*session_id=*/123);

  // Create test bitmap.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(64, 32);
  bitmap.eraseColor(SK_ColorRED);
  std::vector<unsigned char> encoded;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false,
                                    &encoded);

  // Write bitmap to file.
  base::Token token = base::Token::CreateRandom();
  base::WriteFile(profile().GetPath().AppendASCII(
                      token.ToString() +
                      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename),
                  base::as_bytes(base::make_span(
                      std::string(encoded.begin(), encoded.end()))));
  EXPECT_CALL(mock_wallpaper_search_background_manager(),
              SaveCurrentBackgroundToHistory(_))
      .WillOnce(MoveArgAndReturn<0>(&history_entry_arg, token));

  side_panel::customize_chrome::mojom::ResultDescriptorsPtr result_descriptors =
      side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors->subject = "foo";
  result_descriptors->mood = "bar";
  result_descriptors->style = "foobar";
  handler->SetBackgroundToHistoryImage(token, std::move(result_descriptors));
  task_environment().RunUntilIdle();
  task_environment().AdvanceClock(base::Milliseconds(321));

  std::move(decoder_callback).Run(gfx::Image::CreateFrom1xBitmap(bitmap));

  // Check that the bitmap and token passed to
  // |WallpaperSearchBackgroundManager| after the reading the history file
  // and decoding matches the test history image.
  SkBitmap bitmap_arg = image_arg.AsBitmap();
  EXPECT_EQ(token_arg, token);
  EXPECT_EQ(bitmap_arg.getColor(0, 0), bitmap.getColor(0, 0));
  EXPECT_EQ(bitmap_arg.width(), bitmap.width());
  EXPECT_EQ(bitmap_arg.height(), bitmap.height());

  // Check that the processing timer is being passed.
  EXPECT_EQ(timer.Elapsed().InMilliseconds(), 321);

  // Check that the set theme is saved to history on destruction.
  handler.reset();
  EXPECT_EQ(token_arg.ToString(), history_entry_arg.id.ToString());
  EXPECT_EQ("foo", history_entry_arg.subject);
  EXPECT_EQ("bar", history_entry_arg.mood);
  EXPECT_EQ("foobar", history_entry_arg.style);
}

TEST_F(WallpaperSearchHandlerTest, SetBackgroundToWallpaperSearchResult) {
  // Fill wallpaper_search_results_ with 2 bitmaps.
  optimization_guide::proto::WallpaperSearchRequest request;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      done_callback;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback1;
  base::OnceCallback<void(const gfx::Image&)> decoder_callback2;
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _))
      .WillOnce(Invoke(
          [&request, &done_callback](
              optimization_guide::proto::ModelExecutionFeature feature_arg,
              const google::protobuf::MessageLite& request_arg,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  done_callback_arg) {
            ASSERT_EQ(request.GetTypeName(), request_arg.GetTypeName());
            request.CheckTypeAndMergeFrom(request_arg);
            done_callback = std::move(done_callback_arg);
          }));
  EXPECT_CALL(mock_image_decoder(), DecodeImage(_, _, _, _))
      .Times(2)
      .WillOnce(Invoke(
          [&decoder_callback1](const std::string& image_data,
                               const gfx::Size& desired_image_frame_size,
                               data_decoder::DataDecoder* data_decoder,
                               image_fetcher::ImageDecodedCallback callback) {
            decoder_callback1 = std::move(callback);
          }))
      .WillOnce(Invoke(
          [&decoder_callback2](const std::string& image_data,
                               const gfx::Size& desired_image_frame_size,
                               data_decoder::DataDecoder* data_decoder,
                               image_fetcher::ImageDecodedCallback callback) {
            decoder_callback2 = std::move(callback);
          }));
  base::MockCallback<WallpaperSearchHandler::GetWallpaperSearchResultsCallback>
      callback;
  auto handler = MakeHandler(/*session_id=*/123);

  side_panel::customize_chrome::mojom::ResultDescriptorsPtr result_descriptors =
      side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors->subject = "foo";
  handler->GetWallpaperSearchResults(std::move(result_descriptors),
                                     callback.Get());
  EXPECT_EQ("foo", request.descriptors().descriptor_a());
  EXPECT_TRUE(request.descriptors().descriptor_b().empty());
  EXPECT_TRUE(request.descriptors().descriptor_c().empty());
  EXPECT_TRUE(request.descriptors().descriptor_d().empty());

  optimization_guide::proto::WallpaperSearchResponse response;

  // Create test bitmap 1 and add it to response.
  SkBitmap bitmap1;
  bitmap1.allocN32Pixels(32, 32);
  bitmap1.eraseColor(SK_ColorRED);
  std::vector<unsigned char> encoded1;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap1, /*discard_transparency=*/false,
                                    &encoded1);
  auto* image1 = response.add_images();
  image1->set_encoded_image(std::string(encoded1.begin(), encoded1.end()));
  image1->set_image_id(111);

  // Create test bitmap 2 and add it to response.
  SkBitmap bitmap2;
  bitmap2.allocN32Pixels(32, 32);
  bitmap2.eraseColor(SK_ColorBLUE);
  std::vector<unsigned char> encoded2;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap2, /*discard_transparency=*/false,
                                    &encoded2);
  auto* image2 = response.add_images();
  image2->set_encoded_image(std::string(encoded2.begin(), encoded2.end()));
  image2->set_image_id(222);

  // Serialize and set result to later send to done_callback.
  std::string serialized_metadata;
  response.SerializeToString(&serialized_metadata);
  optimization_guide::proto::Any result;
  result.set_value(serialized_metadata);
  result.set_type_url("type.googleapis.com/" + response.GetTypeName());

  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      images;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(MoveArg<1>(&images));

  // Advance clock to test request latency.
  task_environment().AdvanceClock(base::Milliseconds(321));

  std::move(done_callback).Run(base::ok(result), ModelQuality());
  std::move(decoder_callback1).Run(gfx::Image::CreateFrom1xBitmap(bitmap1));
  std::move(decoder_callback2).Run(gfx::Image::CreateFrom1xBitmap(bitmap2));

  ASSERT_EQ(2ul, images.size());

  // Simulate that front-end has received the images and rendered them.
  handler->SetResultRenderTime(
      {images[0]->id, images[1]->id},
      base::Time::Now().InMillisecondsFSinceUnixEpoch());

  // Set background to bitmap2.
  SkBitmap bitmap;
  base::Token token;
  base::ElapsedTimer timer;
  EXPECT_CALL(mock_wallpaper_search_background_manager(),
              SelectLocalBackgroundImage(An<const base::Token&>(),
                                         An<const SkBitmap&>(),
                                         An<base::ElapsedTimer>()))
      .WillOnce(
          DoAll(SaveArg<0>(&token), SaveArg<1>(&bitmap), MoveArg<2>(&timer)));

  auto descriptors =
      side_panel::customize_chrome::mojom::ResultDescriptors::New();
  descriptors->subject = "foo";
  handler->SetBackgroundToWallpaperSearchResult(
      images[1]->id,
      (base::Time::Now() + base::Milliseconds(123))
          .InMillisecondsFSinceUnixEpoch(),
      std::move(descriptors));
  task_environment().AdvanceClock(base::Milliseconds(123));

  // Check that the 2nd bitmap was selected by comparing color, since the
  // 2 bitmaps are different colors.
  EXPECT_EQ(bitmap.getColor(0, 0), bitmap2.getColor(0, 0));
  EXPECT_EQ(token, images[1]->id);

  // Check that the processing timer is being passed.
  EXPECT_EQ(timer.Elapsed().InMilliseconds(), 123);

  // Simulate current background is saved to history.
  HistoryEntry history_entry_arg;
  EXPECT_CALL(mock_wallpaper_search_background_manager(),
              SaveCurrentBackgroundToHistory)
      .WillOnce(
          MoveArgAndReturn(&history_entry_arg, std::make_optional(token)));

  std::vector<
      std::unique_ptr<optimization_guide::proto::WallpaperSearchQuality>>
      qualities;
  ON_CALL(mock_optimization_guide_keyed_service(), UploadModelQualityLogs(_))
      .WillByDefault(Invoke(
          [&qualities](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                           log_entry) {
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_wallpaper_search()
                            ->has_quality_data());
            qualities.push_back(
                std::make_unique<
                    optimization_guide::proto::WallpaperSearchQuality>(
                    log_entry->log_ai_data_request()
                        ->mutable_wallpaper_search()
                        ->quality_data()));
          }));

  // Advance clock to test complete latency.
  task_environment().AdvanceClock(base::Milliseconds(432));

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(123, qualities[0]->session_id());
  EXPECT_EQ(0, qualities[0]->index());
  EXPECT_TRUE(qualities[0]->final_request_in_session());
  EXPECT_EQ(321, qualities[0]->request_latency_ms());
  EXPECT_EQ(555, qualities[0]->complete_latency_ms());
  ASSERT_EQ(2, qualities[0]->images_quality_size());
  EXPECT_EQ(111, qualities[0]->images_quality(0).image_id());
  EXPECT_FALSE(qualities[0]->images_quality(0).previewed());
  EXPECT_FALSE(qualities[0]->images_quality(0).selected());
  EXPECT_EQ(222, qualities[0]->images_quality(1).image_id());
  EXPECT_TRUE(qualities[0]->images_quality(1).previewed());
  EXPECT_TRUE(qualities[0]->images_quality(1).selected());
  EXPECT_EQ(123, qualities[0]->images_quality(1).preview_latency_ms());

  // Set background saves on destruction.
  EXPECT_EQ(history_entry_arg.id, token);
  EXPECT_EQ("foo", history_entry_arg.subject);
}

TEST_F(WallpaperSearchHandlerTest, SetUserFeedback) {
  // Mock first request, then mark as thumbs down.
  optimization_guide::proto::WallpaperSearchRequest request1;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      done_callback1;
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _))
      .WillOnce(Invoke(
          [&request1, &done_callback1](
              optimization_guide::proto::ModelExecutionFeature feature_arg,
              const google::protobuf::MessageLite& request_arg,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  done_callback_arg) {
            request1.CheckTypeAndMergeFrom(request_arg);
            done_callback1 = std::move(done_callback_arg);
          }));
  base::MockCallback<WallpaperSearchHandler::GetWallpaperSearchResultsCallback>
      callback1;
  auto handler = MakeHandler(/*session_id=*/123);
  side_panel::customize_chrome::mojom::ResultDescriptorsPtr result_descriptors =
      side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors->subject = "foo1";
  result_descriptors->style = "bar1";
  result_descriptors->mood = "baz1";
  result_descriptors->color =
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorWHITE);
  handler->GetWallpaperSearchResults(std::move(result_descriptors),
                                     callback1.Get());
  optimization_guide::proto::WallpaperSearchResponse response1;
  std::string serialized_metadata1;
  response1.SerializeToString(&serialized_metadata1);
  optimization_guide::proto::Any result1;
  result1.set_value(serialized_metadata1);
  result1.set_type_url("type.googleapis.com/" + response1.GetTypeName());
  std::move(done_callback1).Run(base::ok(result1), ModelQuality());
#if BUILDFLAG(IS_CHROMEOS)
  // The feedback dialog on CrOS & LaCrOS happens at the system level.
  // This can cause the unittest to crash. LaCrOS has a separate feedback
  // browser test which gives us some coverage.
  handler->SkipShowFeedbackPageForTesting(true);
#endif  // BUILDFLAG(IS_CHROMEOS)
  handler->SetUserFeedback(
      side_panel::customize_chrome::mojom::UserFeedback::kThumbsDown);

  // Mock second request, then mark as thumbs up.
  optimization_guide::proto::WallpaperSearchRequest request2;
  optimization_guide::OptimizationGuideModelExecutionResultCallback
      done_callback2;
  EXPECT_CALL(mock_optimization_guide_keyed_service(), ExecuteModel(_, _, _))
      .WillOnce(Invoke(
          [&request2, &done_callback2](
              optimization_guide::proto::ModelExecutionFeature feature_arg,
              const google::protobuf::MessageLite& request_arg,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  done_callback_arg) {
            ASSERT_EQ(request2.GetTypeName(), request_arg.GetTypeName());
            request2.CheckTypeAndMergeFrom(request_arg);
            done_callback2 = std::move(done_callback_arg);
          }));
  base::MockCallback<WallpaperSearchHandler::GetWallpaperSearchResultsCallback>
      callback2;
  side_panel::customize_chrome::mojom::ResultDescriptorsPtr
      result_descriptors2 =
          side_panel::customize_chrome::mojom::ResultDescriptors::New();
  result_descriptors2->subject = "foo2";
  result_descriptors2->style = "bar2";
  result_descriptors2->mood = "baz2";
  result_descriptors2->color =
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorRED);
  handler->GetWallpaperSearchResults(std::move(result_descriptors2),
                                     callback2.Get());
  optimization_guide::proto::WallpaperSearchResponse response2;
  std::string serialized_metadata2;
  response2.SerializeToString(&serialized_metadata2);
  optimization_guide::proto::Any result2;
  result2.set_value(serialized_metadata2);
  result2.set_type_url("type.googleapis.com/" + response2.GetTypeName());

  std::move(done_callback2).Run(base::ok(result2), ModelQuality());
  handler->SetUserFeedback(
      side_panel::customize_chrome::mojom::UserFeedback::kThumbsUp);

  std::vector<
      std::unique_ptr<optimization_guide::proto::WallpaperSearchQuality>>
      qualities;
  ON_CALL(mock_optimization_guide_keyed_service(), UploadModelQualityLogs(_))
      .WillByDefault(Invoke(
          [&qualities](std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                           log_entry) {
            EXPECT_TRUE(log_entry->log_ai_data_request()
                            ->mutable_wallpaper_search()
                            ->has_quality_data());
            qualities.push_back(
                std::make_unique<
                    optimization_guide::proto::WallpaperSearchQuality>(
                    log_entry->log_ai_data_request()
                        ->mutable_wallpaper_search()
                        ->quality_data()));
          }));

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_DOWN,
            qualities[0]->user_feedback());
  EXPECT_EQ(optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_UP,
            qualities[1]->user_feedback());
}

TEST_F(WallpaperSearchHandlerTest, GetInspirations_Success) {
  side_panel::customize_chrome::mojom::InspirationsPtr inspirations;
  base::MockCallback<WallpaperSearchHandler::GetInspirationsCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&inspirations](side_panel::customize_chrome::mojom::InspirationsPtr
                              inspirations_ptr_arg) {
            inspirations = std::move(inspirations_ptr_arg);
          }));
  SetUpInspirationsResponseWithData(
      R"()]}'
        {"inspiration_a":[
          {"background_image":"foo_1.png","thumbnail_image":"foo_2.png"},
          {"background_image":"bar_1.png","thumbnail_image":"bar_2.png"}
      ]})");
  ASSERT_FALSE(inspirations);

  auto handler = MakeHandler(/*session_id=*/123);
  handler->GetInspirations(callback.Get());
  task_environment().RunUntilIdle();

  EXPECT_TRUE(inspirations);
  const auto& inspiration_a = inspirations->inspiration_a;
  EXPECT_EQ(2u, inspiration_a.size());
  const auto& foo_inspiration = inspiration_a[0];
  EXPECT_EQ(foo_inspiration->background_url,
            base::StrCat({kGstaticBaseURL, "foo_1.png"}));
  EXPECT_EQ(foo_inspiration->thumbnail_url,
            base::StrCat({kGstaticBaseURL, "foo_2.png"}));
  const auto& bar_inspiration = inspiration_a[1];
  EXPECT_EQ(bar_inspiration->background_url,
            base::StrCat({kGstaticBaseURL, "bar_1.png"}));
  EXPECT_EQ(bar_inspiration->thumbnail_url,
            base::StrCat({kGstaticBaseURL, "bar_2.png"}));
}

TEST_F(WallpaperSearchHandlerTest,
       GetInspirations_Failure_InspirationsFormatIncorrect) {
  side_panel::customize_chrome::mojom::InspirationsPtr inspirations;
  base::MockCallback<WallpaperSearchHandler::GetInspirationsCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&inspirations](side_panel::customize_chrome::mojom::InspirationsPtr
                              inspirations_ptr_arg) {
            inspirations = std::move(inspirations_ptr_arg);
          }));
  SetUpInspirationsResponseWithData(
      R"()]}'
        {"inspiration_a":[
          {"background_image":"foo_1.png"},
          {"thumbnail_image":"bar_2.png"}
      ]})");
  ASSERT_FALSE(inspirations);

  auto handler = MakeHandler(/*session_id=*/123);
  handler->GetInspirations(callback.Get());
  task_environment().RunUntilIdle();

  EXPECT_FALSE(inspirations);
}

TEST_F(WallpaperSearchHandlerTest, GetInspirations_Failure_DataUnreachable) {
  side_panel::customize_chrome::mojom::InspirationsPtr inspirations;
  base::MockCallback<WallpaperSearchHandler::GetInspirationsCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&inspirations](side_panel::customize_chrome::mojom::InspirationsPtr
                              inspirations_ptr_arg) {
            inspirations = std::move(inspirations_ptr_arg);
          }));
  SetUpInspirationsResponseWithNetworkError();
  ASSERT_FALSE(inspirations);

  auto handler = MakeHandler(/*session_id=*/123);
  handler->GetInspirations(callback.Get());
  task_environment().RunUntilIdle();

  EXPECT_FALSE(inspirations);
}
