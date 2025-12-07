// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_base_service.h"

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/paint_preview/browser/paint_preview_base_service_test_factory.h"
#include "components/paint_preview/browser/paint_preview_file_mixin.h"
#include "components/paint_preview/common/mock_paint_preview_recorder.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/mojom/paint_preview_types.mojom.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "components/paint_preview/common/test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace paint_preview {

using testing::Optional;
using testing::Property;

namespace {

const char kTestFeatureDir[] = "test_feature";

class RejectionPaintPreviewPolicy : public PaintPreviewPolicy {
 public:
  RejectionPaintPreviewPolicy() = default;
  ~RejectionPaintPreviewPolicy() override = default;

  RejectionPaintPreviewPolicy(const RejectionPaintPreviewPolicy&) = delete;
  RejectionPaintPreviewPolicy& operator=(const RejectionPaintPreviewPolicy&) =
      delete;

  bool SupportedForContents(content::WebContents* web_contents) override {
    return false;
  }
};

// Builds a PaintPreviewBaseService associated with |key| which will never
// permit paint previews.
std::unique_ptr<KeyedService> BuildServiceWithRejectionPolicy(
    SimpleFactoryKey* key) {
  return std::make_unique<PaintPreviewBaseService>(
      std::make_unique<PaintPreviewFileMixin>(key->GetPath(), kTestFeatureDir),
      std::make_unique<RejectionPaintPreviewPolicy>(), key->IsOffTheRecord());
}

base::FilePath CreateDir(scoped_refptr<FileManager> manager,
                         const DirectoryKey& key) {
  base::test::TestFuture<std::optional<base::FilePath>> future;
  manager->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, manager, key, false),
      future.GetCallback());
  EXPECT_THAT(future.Get(), Optional(Property(&base::FilePath::empty, false)));
  return future.Take().value();
}

}  // namespace

class NoGuidMockPaintPreviewRecorder : public MockPaintPreviewRecorder {
 public:
  NoGuidMockPaintPreviewRecorder() = default;
  ~NoGuidMockPaintPreviewRecorder() override = default;

 protected:
  void CheckParams(
      const mojom::PaintPreviewCaptureParamsPtr& input_params) override {
    // Ignore GUID and File as this is internal information not known by the
    // Keyed Service API.
    EXPECT_EQ(input_params->geometry_metadata_params->clip_rect,
              expected_params_->geometry_metadata_params->clip_rect);
    if (input_params->is_main_frame) {
      EXPECT_EQ(
          input_params->geometry_metadata_params->clip_x_coord_override,
          expected_params_->geometry_metadata_params->clip_x_coord_override);
      EXPECT_EQ(
          input_params->geometry_metadata_params->clip_y_coord_override,
          expected_params_->geometry_metadata_params->clip_y_coord_override);
    } else {
      EXPECT_EQ(input_params->geometry_metadata_params->clip_x_coord_override,
                mojom::ClipCoordOverride::kNone);
      EXPECT_EQ(input_params->geometry_metadata_params->clip_y_coord_override,
                mojom::ClipCoordOverride::kNone);
    }
    EXPECT_EQ(input_params->is_main_frame, expected_params_->is_main_frame);
  }
};

class PaintPreviewBaseServiceTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<RecordingPersistence> {
 public:
  PaintPreviewBaseServiceTest() = default;
  ~PaintPreviewBaseServiceTest() override = default;

  PaintPreviewBaseServiceTest(const PaintPreviewBaseService&) = delete;
  PaintPreviewBaseServiceTest& operator=(const PaintPreviewBaseService&) =
      delete;

 protected:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    key_ = std::make_unique<SimpleFactoryKey>(
        browser_context()->GetPath(), browser_context()->IsOffTheRecord());
    PaintPreviewBaseServiceTestFactory::GetInstance()->SetTestingFactory(
        key_.get(),
        base::BindRepeating(&PaintPreviewBaseServiceTestFactory::Build));

    rejection_policy_key_ = std::make_unique<SimpleFactoryKey>(
        browser_context()->GetPath(), browser_context()->IsOffTheRecord());
    PaintPreviewBaseServiceTestFactory::GetInstance()->SetTestingFactory(
        rejection_policy_key_.get(),
        base::BindRepeating(&BuildServiceWithRejectionPolicy));
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://www.chromium.org"));
  }

  void OverrideInterface(NoGuidMockPaintPreviewRecorder* service) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        main_rfh()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::PaintPreviewRecorder::Name_,
        base::BindRepeating(&NoGuidMockPaintPreviewRecorder::BindRequest,
                            base::Unretained(service)));
  }

  PaintPreviewBaseService* GetService() {
    return PaintPreviewBaseServiceTestFactory::GetForKey(key_.get());
  }

  PaintPreviewBaseService* GetServiceWithRejectionPolicy() {
    return PaintPreviewBaseServiceTestFactory::GetForKey(
        rejection_policy_key_.get());
  }

  PaintPreviewBaseService::CaptureParams CreateCaptureParams(
      content::WebContents* web_contents,
      base::FilePath* root_dir,
      RecordingPersistence persistence,
      gfx::Rect clip_rect,
      mojom::ClipCoordOverride clip_x_coord_override,
      mojom::ClipCoordOverride clip_y_coord_override,
      bool capture_links,
      size_t max_per_capture_size,
      uint64_t max_decoded_image_size_bytes) {
    PaintPreviewBaseService::CaptureParams capture_params;
    capture_params.web_contents = web_contents;
    capture_params.root_dir = root_dir;
    capture_params.persistence = persistence;
    capture_params.clip_rect = clip_rect;
    capture_params.clip_x_coord_override = clip_x_coord_override;
    capture_params.clip_y_coord_override = clip_y_coord_override;
    capture_params.capture_links = capture_links;
    capture_params.max_per_capture_size = max_per_capture_size;
    capture_params.max_decoded_image_size_bytes = max_decoded_image_size_bytes;
    return capture_params;
  }

 private:
  std::unique_ptr<SimpleFactoryKey> key_;
  std::unique_ptr<SimpleFactoryKey> rejection_policy_key_;
};

TEST_P(PaintPreviewBaseServiceTest, CaptureMainFrame) {
  NoGuidMockPaintPreviewRecorder recorder;
  auto params = mojom::PaintPreviewCaptureParams::New();
  params->geometry_metadata_params = mojom::GeometryMetadataParams::New();
  params->geometry_metadata_params->clip_rect = gfx::Rect(0, 0, 0, 0);
  params->geometry_metadata_params->clip_x_coord_override =
      mojom::ClipCoordOverride::kCenterOnScrollOffset;
  params->geometry_metadata_params->clip_y_coord_override =
      mojom::ClipCoordOverride::kScrollOffset;
  params->is_main_frame = true;
  params->max_capture_size = 50;
  params->max_decoded_image_size_bytes = 1000;
  recorder.SetExpectedParams(std::move(params));
  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->geometry_metadata = mojom::GeometryMetadataResponse::New();
  response->embedding_token = std::nullopt;
  if (GetParam() == RecordingPersistence::kMemoryBuffer) {
    response->skp.emplace(mojo_base::BigBuffer());
  }
  recorder.SetResponse(std::move(response));
  OverrideInterface(&recorder);

  auto* service = GetService();
  EXPECT_FALSE(service->IsOffTheRecord());
  auto manager = service->GetFileMixin()->GetFileManager();
  base::FilePath path = CreateDir(
      manager, manager->CreateKey(web_contents()->GetLastCommittedURL()));

  base::RunLoop loop;
  service->CapturePaintPreview(
      CreateCaptureParams(
          web_contents(), &path, GetParam(), gfx::Rect(0, 0, 0, 0),
          mojom::ClipCoordOverride::kCenterOnScrollOffset,
          mojom::ClipCoordOverride::kScrollOffset, true, 50, 1000),
      base::BindLambdaForTesting(
          [&](PaintPreviewBaseService::CaptureStatus status,
              std::unique_ptr<CaptureResult> result) {
            EXPECT_EQ(status, PaintPreviewBaseService::CaptureStatus::kOk);
            EXPECT_TRUE(result->proto.has_root_frame());
            EXPECT_EQ(result->proto.subframes_size(), 0);
            EXPECT_TRUE(result->proto.root_frame().is_main_frame());
            auto token = base::UnguessableToken::Deserialize(
                             result->proto.root_frame().embedding_token_high(),
                             result->proto.root_frame().embedding_token_low())
                             .value();
            switch (GetParam()) {
              case RecordingPersistence::kFileSystem: {
#if BUILDFLAG(IS_WIN)
                base::FilePath received_path = base::FilePath(
                    base::UTF8ToWide(result->proto.root_frame().file_path()));
                base::FilePath name(
                    base::UTF8ToWide(base::StrCat({token.ToString(), ".skp"})));
#else
                base::FilePath received_path =
                    base::FilePath(result->proto.root_frame().file_path());
                base::FilePath name(base::StrCat({token.ToString(), ".skp"}));
#endif
                EXPECT_EQ(received_path.DirName(), path);
                EXPECT_EQ(received_path.BaseName(), name);
              } break;

              case RecordingPersistence::kMemoryBuffer: {
                EXPECT_EQ(result->serialized_skps.size(), 1u);
                EXPECT_TRUE(result->serialized_skps.contains(token));
              } break;

              default:
                NOTREACHED();
            }
            loop.Quit();
          }));
  loop.Run();
}

TEST_P(PaintPreviewBaseServiceTest, CaptureFailed) {
  NoGuidMockPaintPreviewRecorder recorder;
  auto params = mojom::PaintPreviewCaptureParams::New();
  params->geometry_metadata_params = mojom::GeometryMetadataParams::New();
  params->geometry_metadata_params->clip_rect = gfx::Rect(0, 0, 0, 0);
  params->geometry_metadata_params->clip_x_coord_override =
      mojom::ClipCoordOverride::kCenterOnScrollOffset;
  params->geometry_metadata_params->clip_y_coord_override =
      mojom::ClipCoordOverride::kScrollOffset;
  params->is_main_frame = true;
  params->max_capture_size = 0;
  recorder.SetExpectedParams(std::move(params));
  recorder.SetResponse(base::unexpected(mojom::PaintPreviewStatus::kFailed));
  OverrideInterface(&recorder);

  auto* service = GetService();
  EXPECT_FALSE(service->IsOffTheRecord());
  auto manager = service->GetFileMixin()->GetFileManager();
  base::FilePath path = CreateDir(
      manager, manager->CreateKey(web_contents()->GetLastCommittedURL()));

  base::RunLoop loop;
  service->CapturePaintPreview(
      CreateCaptureParams(web_contents(), &path, GetParam(),
                          gfx::Rect(0, 0, 0, 0),
                          mojom::ClipCoordOverride::kCenterOnScrollOffset,
                          mojom::ClipCoordOverride::kScrollOffset, true, 0,
                          std::numeric_limits<uint64_t>::max()),
      base::BindLambdaForTesting(
          [&](PaintPreviewBaseService::CaptureStatus status,
              std::unique_ptr<CaptureResult> result) {
            EXPECT_EQ(status,
                      PaintPreviewBaseService::CaptureStatus::kCaptureFailed);
            EXPECT_EQ(result, nullptr);
            loop.Quit();
          }));
  loop.Run();
}

TEST_P(PaintPreviewBaseServiceTest, CaptureDisallowed) {
  NoGuidMockPaintPreviewRecorder recorder;
  auto params = mojom::PaintPreviewCaptureParams::New();
  params->geometry_metadata_params = mojom::GeometryMetadataParams::New();
  params->geometry_metadata_params->clip_rect = gfx::Rect(0, 0, 0, 0);
  params->geometry_metadata_params->clip_x_coord_override =
      mojom::ClipCoordOverride::kCenterOnScrollOffset;
  params->geometry_metadata_params->clip_y_coord_override =
      mojom::ClipCoordOverride::kScrollOffset;
  params->is_main_frame = true;
  params->max_capture_size = 0;
  recorder.SetExpectedParams(std::move(params));
  OverrideInterface(&recorder);

  auto* service = GetServiceWithRejectionPolicy();
  EXPECT_FALSE(service->IsOffTheRecord());
  auto manager = service->GetFileMixin()->GetFileManager();
  base::FilePath path = CreateDir(
      manager, manager->CreateKey(web_contents()->GetLastCommittedURL()));

  base::RunLoop loop;
  service->CapturePaintPreview(
      CreateCaptureParams(web_contents(), &path, GetParam(),
                          gfx::Rect(0, 0, 0, 0),
                          mojom::ClipCoordOverride::kCenterOnScrollOffset,
                          mojom::ClipCoordOverride::kScrollOffset, true, 0,
                          std::numeric_limits<uint64_t>::max()),
      base::BindLambdaForTesting(
          [&](PaintPreviewBaseService::CaptureStatus status,
              std::unique_ptr<CaptureResult> result) {
            EXPECT_EQ(
                status,
                PaintPreviewBaseService::CaptureStatus::kContentUnsupported);
            EXPECT_EQ(result, nullptr);
            loop.Quit();
          }));
  loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PaintPreviewBaseServiceTest,
    testing::Values(RecordingPersistence::kFileSystem,
                    RecordingPersistence::kMemoryBuffer),
    [](const testing::TestParamInfo<RecordingPersistence>& info) {
      return std::string(PersistenceToString(info.param));
    });

}  // namespace paint_preview
