// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/token.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/image_fetcher/core/mock_image_decoder.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/wallpaper_search.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search/ntp_features.h"
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

class MockNtpCustomBackgroundService : public NtpCustomBackgroundService {
 public:
  explicit MockNtpCustomBackgroundService(Profile* profile)
      : NtpCustomBackgroundService(profile) {}
  MOCK_METHOD(void,
              SelectLocalBackgroundImage,
              (const base::Token&, const SkBitmap&));
};

std::unique_ptr<TestingProfile> MakeTestingProfile(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      OptimizationGuideKeyedServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            testing::NiceMock<MockOptimizationGuideKeyedService>>(context);
      }));
  profile_builder.AddTestingFactory(
      NtpCustomBackgroundServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            testing::NiceMock<MockNtpCustomBackgroundService>>(
            Profile::FromBrowserContext(context));
      }));
  profile_builder.SetSharedURLLoaderFactory(url_loader_factory);
  auto profile = profile_builder.Build();
  return profile;
}

}  // namespace

class WallpaperSearchHandlerTest : public testing::Test {
 public:
  WallpaperSearchHandlerTest()
      : profile_(MakeTestingProfile(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_))),
        mock_optimization_guide_keyed_service_(
            static_cast<MockOptimizationGuideKeyedService*>(
                OptimizationGuideKeyedServiceFactory::GetForProfile(
                    profile_.get()))),
        mock_ntp_custom_background_service_(
            static_cast<MockNtpCustomBackgroundService*>(
                NtpCustomBackgroundServiceFactory::GetForProfile(
                    profile_.get()))),
        handler_(
            mojo::PendingReceiver<
                side_panel::customize_chrome::mojom::WallpaperSearchHandler>(),
            profile_.get(),
            &mock_image_decoder_) {}

  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kCustomizeChromeWallpaperSearch,
                              optimization_guide::features::
                                  kOptimizationGuideModelExecution},
        /*disabled_features=*/{});
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    RegisterLocalState(local_state_.registry());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
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

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  WallpaperSearchHandler& handler() { return handler_; }
  MockOptimizationGuideKeyedService& mock_optimization_guide_keyed_service() {
    return *mock_optimization_guide_keyed_service_;
  }
  MockNtpCustomBackgroundService& mock_ntp_custom_background_service() {
    return *mock_ntp_custom_background_service_;
  }
  image_fetcher::MockImageDecoder& mock_image_decoder() {
    return mock_image_decoder_;
  }

 private:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
  raw_ptr<MockNtpCustomBackgroundService> mock_ntp_custom_background_service_;
  image_fetcher::MockImageDecoder mock_image_decoder_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  WallpaperSearchHandler handler_;
};

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

  ASSERT_FALSE(descriptors);
  handler().GetDescriptors(callback.Get());
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

  handler().GetDescriptors(callback.Get());
  handler().GetDescriptors(callback_2.Get());
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

  handler().GetDescriptors(callback.Get());
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

  handler().GetDescriptors(callback.Get());
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

  handler().GetDescriptors(callback.Get());
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

  handler().GetWallpaperSearchResults(
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
  response.add_images()->set_encoded_image(
      std::string(encoded1.begin(), encoded1.end()));

  // Create test bitmap 2 and add it to response.
  SkBitmap bitmap2;
  bitmap2.allocN32Pixels(32, 32);
  bitmap2.eraseColor(SK_ColorBLUE);
  std::vector<unsigned char> encoded2;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap2, /*discard_transparency=*/false,
                                    &encoded2);
  response.add_images()->set_encoded_image(
      std::string(encoded2.begin(), encoded2.end()));

  // Serialize and set result to later send to done_callback.
  std::string serialized_metadata;
  response.SerializeToString(&serialized_metadata);
  optimization_guide::proto::Any result;
  result.set_value(serialized_metadata);
  result.set_type_url("type.googleapis.com/" + response.GetTypeName());

  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      images;
  EXPECT_CALL(callback, Run(_)).WillOnce(MoveArg(&images));

  std::move(done_callback).Run(base::ok(result));

  std::move(decoder_callback1).Run(gfx::Image::CreateFrom1xBitmap(bitmap1));
  std::move(decoder_callback2).Run(gfx::Image::CreateFrom1xBitmap(bitmap2));

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
  handler().GetWallpaperSearchResults(
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
  handler().GetWallpaperSearchResults(
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

  handler().GetWallpaperSearchResults("foo", absl::nullopt, absl::nullopt,
                                      nullptr, callback.Get());
  EXPECT_EQ("foo", request.descriptors().descriptor_a());
  EXPECT_TRUE(request.descriptors().descriptor_b().empty());
  EXPECT_TRUE(request.descriptors().descriptor_c().empty());
  EXPECT_TRUE(request.descriptors().descriptor_d().empty());

  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      images;
  EXPECT_CALL(callback, Run(_)).WillOnce(MoveArg(&images));

  std::move(done_callback)
      .Run(base::unexpected(
          optimization_guide::OptimizationGuideModelExecutionError::
              FromModelExecutionError(
                  optimization_guide::OptimizationGuideModelExecutionError::
                      ModelExecutionError::kGenericFailure)));

  EXPECT_EQ(images.size(), 0u);
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

  handler().GetWallpaperSearchResults("foo", absl::nullopt, absl::nullopt,
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
  EXPECT_CALL(callback, Run(_)).WillOnce(MoveArg(&images));

  std::move(done_callback).Run(base::ok(result));

  EXPECT_EQ(static_cast<int>(images.size()), response.images_size());
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

  handler().GetWallpaperSearchResults("foo", absl::nullopt, absl::nullopt,
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
  response.add_images()->set_encoded_image(
      std::string(encoded1.begin(), encoded1.end()));

  // Create test bitmap 2 and add it to response.
  SkBitmap bitmap2;
  bitmap2.allocN32Pixels(32, 32);
  bitmap2.eraseColor(SK_ColorBLUE);
  std::vector<unsigned char> encoded2;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap2, /*discard_transparency=*/false,
                                    &encoded2);
  response.add_images()->set_encoded_image(
      std::string(encoded2.begin(), encoded2.end()));

  // Serialize and set result to later send to done_callback.
  std::string serialized_metadata;
  response.SerializeToString(&serialized_metadata);
  optimization_guide::proto::Any result;
  result.set_value(serialized_metadata);
  result.set_type_url("type.googleapis.com/" + response.GetTypeName());

  std::vector<side_panel::customize_chrome::mojom::WallpaperSearchResultPtr>
      images;
  EXPECT_CALL(callback, Run(_)).WillOnce(MoveArg(&images));

  std::move(done_callback).Run(base::ok(result));

  std::move(decoder_callback1).Run(gfx::Image::CreateFrom1xBitmap(bitmap1));
  std::move(decoder_callback2).Run(gfx::Image::CreateFrom1xBitmap(bitmap2));

  // Set background to bitmap2.
  SkBitmap bitmap;
  base::Token token;
  EXPECT_CALL(mock_ntp_custom_background_service(),
              SelectLocalBackgroundImage(An<const base::Token&>(),
                                         An<const SkBitmap&>()))
      .WillOnce(DoAll(SaveArg<0>(&token), SaveArg<1>(&bitmap)));

  handler().SetBackgroundToWallpaperSearchResult(images[1]->id);

  // Check that the 2nd bitmap was selected by comparing color, since the
  // 2 bitmaps are different colors.
  EXPECT_EQ(bitmap.getColor(0, 0), bitmap2.getColor(0, 0));
  EXPECT_EQ(token, images[1]->id);
}
