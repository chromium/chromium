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
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/token.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_background_manager.h"
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
  MOCK_METHOD0(GetHistory, std::vector<base::Token>());
  MOCK_METHOD2(SelectHistoryImage, void(const base::Token&, const gfx::Image&));
  MOCK_METHOD2(SelectLocalBackgroundImage,
               void(const base::Token&, const SkBitmap&));
  MOCK_METHOD0(SaveCurrentBackgroundToHistory, absl::optional<base::Token>());
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

class FakeModelQualityLogEntry
    : public optimization_guide::ModelQualityLogEntry {
 public:
  using DestructionCallback = base::OnceCallback<void(
      const optimization_guide::proto::WallpaperSearchQuality&)>;

  explicit FakeModelQualityLogEntry(DestructionCallback destruction_callback)
      : optimization_guide::ModelQualityLogEntry(
            std::make_unique<optimization_guide::proto::LogAiDataRequest>()),
        destruction_callback_(std::move(destruction_callback)) {}

  ~FakeModelQualityLogEntry() override {
    std::move(destruction_callback_)
        .Run(
            *quality_data<optimization_guide::WallpaperSearchFeatureTypeMap>());
  }

 private:
  DestructionCallback destruction_callback_;
};

std::unique_ptr<FakeModelQualityLogEntry> SaveQuality(
    optimization_guide::proto::WallpaperSearchQuality* out_quality) {
  return std::make_unique<FakeModelQualityLogEntry>(base::BindOnce(
      [](optimization_guide::proto::WallpaperSearchQuality* out_quality,
         const optimization_guide::proto::WallpaperSearchQuality& quality) {
        *out_quality = quality;
      },
      out_quality));
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

  const std::string kDescriptorsBaseURL =
      "https://static.corp.google.com/chrome-wallpaper-search/";
  const std::string kDescriptorsLoadURL =
      base::StrCat({kDescriptorsBaseURL, "descriptors_en-US.json"});
  void SetUpDescriptorsResponseWithData(const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {}));
    test_url_loader_factory_.AddResponse(kDescriptorsLoadURL, response);
  }
  void SetUpDescriptorsResponseWithNetworkError() {
    test_url_loader_factory_.AddResponse(kDescriptorsLoadURL, std::string(),
                                         net::HTTP_NOT_FOUND);
  }

  std::unique_ptr<WallpaperSearchHandler> MakeHandler(int64_t session_id) {
    EXPECT_CALL(mock_wallpaper_search_background_manager(),
                SaveCurrentBackgroundToHistory);
    auto handler = std::make_unique<WallpaperSearchHandler>(
        mojo::PendingReceiver<
            side_panel::customize_chrome::mojom::WallpaperSearchHandler>(),
        mock_client_.BindAndGetRemote(), profile_.get(), &mock_image_decoder_,
        &mock_wallpaper_search_background_manager_, session_id);
    mock_client_.FlushForTesting();
    return handler;
  }

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
  std::vector<base::Token> history;
  history.push_back(token);
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
  EXPECT_EQ(base::StrCat({kDescriptorsBaseURL, "bar.png"}),
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
  EXPECT_EQ(base::StrCat({kDescriptorsBaseURL, "bar.png"}),
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

  handler->GetWallpaperSearchResults(
      "foo", "bar", "baz",
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorWHITE),
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
  optimization_guide::proto::WallpaperSearchQuality quality;

  std::move(done_callback).Run(base::ok(result), SaveQuality(&quality));

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

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(123, quality.session_id());
  EXPECT_EQ(0, quality.index());
  EXPECT_TRUE(quality.final_request_in_session());
  EXPECT_EQ(321, quality.request_latency_ms());
  ASSERT_EQ(2, quality.images_quality_size());
  EXPECT_EQ(111, quality.images_quality(0).image_id());
  EXPECT_FALSE(quality.images_quality(0).previewed());
  EXPECT_FALSE(quality.images_quality(0).selected());
  EXPECT_EQ(222, quality.images_quality(1).image_id());
  EXPECT_FALSE(quality.images_quality(1).previewed());
  EXPECT_FALSE(quality.images_quality(1).selected());
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

  handler->GetWallpaperSearchResults(
      "foo1", "bar1", "baz1",
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorWHITE),
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
  optimization_guide::proto::WallpaperSearchQuality quality1;

  std::move(done_callback1).Run(base::ok(result1), SaveQuality(&quality1));

  ASSERT_EQ(status1,
            side_panel::customize_chrome::mojom::WallpaperSearchStatus::kError);
  ASSERT_EQ(static_cast<int>(images1.size()), response1.images_size());

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

  handler->GetWallpaperSearchResults(
      "foo2", "bar2", "baz2",
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorRED),
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
  optimization_guide::proto::WallpaperSearchQuality quality2;

  std::move(done_callback2).Run(base::ok(result2), SaveQuality(&quality2));

  ASSERT_EQ(status2,
            side_panel::customize_chrome::mojom::WallpaperSearchStatus::kError);
  ASSERT_EQ(static_cast<int>(images2.size()), response2.images_size());

  // Quality logs on destruction and when a second request is made.
  handler.reset();
  // First request.
  EXPECT_EQ(123, quality1.session_id());
  EXPECT_EQ(0, quality1.index());
  EXPECT_FALSE(quality1.final_request_in_session());
  EXPECT_EQ(321, quality1.request_latency_ms());
  EXPECT_EQ(0, quality1.images_quality_size());
  // Second request.
  EXPECT_EQ(123, quality2.session_id());
  EXPECT_EQ(1, quality2.index());
  EXPECT_TRUE(quality2.final_request_in_session());
  EXPECT_EQ(456, quality2.request_latency_ms());
  EXPECT_EQ(0, quality2.images_quality_size());
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

  handler->GetWallpaperSearchResults(
      "foo", absl::nullopt, absl::nullopt,
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorRED),
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

  handler->GetWallpaperSearchResults(
      "foo", absl::nullopt, absl::nullopt,
      side_panel::customize_chrome::mojom::DescriptorDValue::NewHue(0),
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

  handler->GetWallpaperSearchResults("foo", absl::nullopt, absl::nullopt,
                                     nullptr, callback.Get());
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
  optimization_guide::proto::WallpaperSearchQuality quality;

  std::move(done_callback)
      .Run(
          base::unexpected(
              optimization_guide::OptimizationGuideModelExecutionError::
                  FromModelExecutionError(
                      optimization_guide::OptimizationGuideModelExecutionError::
                          ModelExecutionError::kGenericFailure)),
          SaveQuality(&quality));

  EXPECT_EQ(status,
            side_panel::customize_chrome::mojom::WallpaperSearchStatus::kError);
  EXPECT_EQ(images.size(), 0u);

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(123, quality.session_id());
  EXPECT_EQ(0, quality.index());
  EXPECT_TRUE(quality.final_request_in_session());
  EXPECT_EQ(321, quality.request_latency_ms());
  EXPECT_EQ(0, quality.images_quality_size());
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

  handler->GetWallpaperSearchResults("foo", absl::nullopt, absl::nullopt,
                                     nullptr, callback.Get());
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
  optimization_guide::proto::WallpaperSearchQuality quality;

  std::move(done_callback).Run(base::ok(result), SaveQuality(&quality));

  EXPECT_EQ(status,
            side_panel::customize_chrome::mojom::WallpaperSearchStatus::kError);
  EXPECT_EQ(static_cast<int>(images.size()), response.images_size());

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(123, quality.session_id());
  EXPECT_EQ(0, quality.index());
  EXPECT_TRUE(quality.final_request_in_session());
  EXPECT_EQ(321, quality.request_latency_ms());
  EXPECT_EQ(0, quality.images_quality_size());
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

  handler->GetWallpaperSearchResults("foo", absl::nullopt, absl::nullopt,
                                     nullptr, callback.Get());
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
  optimization_guide::proto::WallpaperSearchQuality quality;

  std::move(done_callback)
      .Run(
          base::unexpected(
              optimization_guide::OptimizationGuideModelExecutionError::
                  FromModelExecutionError(
                      optimization_guide::OptimizationGuideModelExecutionError::
                          ModelExecutionError::kRequestThrottled)),
          SaveQuality(&quality));

  EXPECT_EQ(status, side_panel::customize_chrome::mojom::WallpaperSearchStatus::
                        kRequestThrottled);
  EXPECT_EQ(images.size(), 0u);

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(123, quality.session_id());
  EXPECT_EQ(0, quality.index());
  EXPECT_TRUE(quality.final_request_in_session());
  EXPECT_EQ(321, quality.request_latency_ms());
  EXPECT_EQ(0, quality.images_quality_size());
}

TEST_F(WallpaperSearchHandlerTest, SetBackgroundToHistoryImage) {
  base::OnceCallback<void(const gfx::Image&)> decoder_callback;
  base::Token token_arg;
  gfx::Image image_arg;
  EXPECT_CALL(mock_wallpaper_search_background_manager(),
              SelectHistoryImage(_, _))
      .WillOnce(DoAll(MoveArg<0>(&token_arg), MoveArg<1>(&image_arg)));
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

  handler->SetBackgroundToHistoryImage(token);
  task_environment().RunUntilIdle();

  std::move(decoder_callback).Run(gfx::Image::CreateFrom1xBitmap(bitmap));

  // Check that the bitmap and token passed to
  // |WallpaperSearchBackgroundManager| after the reading the history file
  // and decoding matches the test history image.
  SkBitmap bitmap_arg = image_arg.AsBitmap();
  EXPECT_EQ(token_arg, token);
  EXPECT_EQ(bitmap_arg.getColor(0, 0), bitmap.getColor(0, 0));
  EXPECT_EQ(bitmap_arg.width(), bitmap.width());
  EXPECT_EQ(bitmap_arg.height(), bitmap.height());
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

  handler->GetWallpaperSearchResults("foo", absl::nullopt, absl::nullopt,
                                     nullptr, callback.Get());
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
  optimization_guide::proto::WallpaperSearchQuality quality;

  std::move(done_callback).Run(base::ok(result), SaveQuality(&quality));

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
  EXPECT_CALL(mock_wallpaper_search_background_manager(),
              SelectLocalBackgroundImage(An<const base::Token&>(),
                                         An<const SkBitmap&>()))
      .WillOnce(DoAll(SaveArg<0>(&token), SaveArg<1>(&bitmap)));

  handler->SetBackgroundToWallpaperSearchResult(
      images[1]->id, (base::Time::Now() + base::Milliseconds(123))
                         .InMillisecondsFSinceUnixEpoch());

  // Check that the 2nd bitmap was selected by comparing color, since the
  // 2 bitmaps are different colors.
  EXPECT_EQ(bitmap.getColor(0, 0), bitmap2.getColor(0, 0));
  EXPECT_EQ(token, images[1]->id);

  // Simulate current background is saved to history.
  ON_CALL(mock_wallpaper_search_background_manager(),
          SaveCurrentBackgroundToHistory)
      .WillByDefault(Return(absl::make_optional(token)));

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(123, quality.session_id());
  EXPECT_EQ(0, quality.index());
  EXPECT_TRUE(quality.final_request_in_session());
  EXPECT_EQ(321, quality.request_latency_ms());
  ASSERT_EQ(2, quality.images_quality_size());
  EXPECT_EQ(111, quality.images_quality(0).image_id());
  EXPECT_FALSE(quality.images_quality(0).previewed());
  EXPECT_FALSE(quality.images_quality(0).selected());
  EXPECT_EQ(222, quality.images_quality(1).image_id());
  EXPECT_TRUE(quality.images_quality(1).previewed());
  EXPECT_TRUE(quality.images_quality(1).selected());
  EXPECT_EQ(123, quality.images_quality(1).preview_latency_ms());
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
  handler->GetWallpaperSearchResults(
      "foo1", "bar1", "baz1",
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorWHITE),
      callback1.Get());
  optimization_guide::proto::WallpaperSearchResponse response1;
  std::string serialized_metadata1;
  response1.SerializeToString(&serialized_metadata1);
  optimization_guide::proto::Any result1;
  result1.set_value(serialized_metadata1);
  result1.set_type_url("type.googleapis.com/" + response1.GetTypeName());
  optimization_guide::proto::WallpaperSearchQuality quality1;
  std::move(done_callback1).Run(base::ok(result1), SaveQuality(&quality1));
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
  handler->GetWallpaperSearchResults(
      "foo2", "bar2", "baz2",
      side_panel::customize_chrome::mojom::DescriptorDValue::NewColor(
          SK_ColorRED),
      callback2.Get());
  optimization_guide::proto::WallpaperSearchResponse response2;
  std::string serialized_metadata2;
  response2.SerializeToString(&serialized_metadata2);
  optimization_guide::proto::Any result2;
  result2.set_value(serialized_metadata2);
  result2.set_type_url("type.googleapis.com/" + response2.GetTypeName());
  optimization_guide::proto::WallpaperSearchQuality quality2;
  std::move(done_callback2).Run(base::ok(result2), SaveQuality(&quality2));
  handler->SetUserFeedback(
      side_panel::customize_chrome::mojom::UserFeedback::kThumbsUp);

  // Quality logs on destruction.
  handler.reset();
  EXPECT_EQ(optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_DOWN,
            quality1.user_feedback());
  EXPECT_EQ(optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_UP,
            quality2.user_feedback());
}
