// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_base_service.h"

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/paint_preview/browser/paint_preview_base_service_test_factory.h"
#include "components/paint_preview/browser/paint_preview_file_mixin.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "components/paint_preview/common/test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif

namespace paint_preview {

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
  base::FilePath out;
  base::RunLoop loop;
  manager->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, manager, key, false),
      base::BindOnce(
          [](base::OnceClosure quit, base::FilePath* out,
             const std::optional<base::FilePath>& path) {
            EXPECT_TRUE(path.has_value());
            EXPECT_FALSE(path->empty());
            *out = path.value();
            std::move(quit).Run();
          },
          loop.QuitClosure(), &out));
  loop.Run();
  return out;
}

}  // namespace

class MockPaintPreviewRecorder : public mojom::PaintPreviewRecorder {
 public:
  MockPaintPreviewRecorder() = default;
  ~MockPaintPreviewRecorder() override = default;

  void CapturePaintPreview(
      mojom::PaintPreviewCaptureParamsPtr params,
      mojom::PaintPreviewRecorder::CapturePaintPreviewCallback callback)
      override {
    {
      base::ScopedAllowBlockingForTesting scope;
      CheckParams(std::move(params));
      std::move(callback).Run(status_, std::move(response_));
    }
  }

  void SetExpectedParams(mojom::PaintPreviewCaptureParamsPtr params) {
    expected_params_ = std::move(params);
  }

  void SetResponse(mojom::PaintPreviewStatus status,
                   mojom::PaintPreviewCaptureResponsePtr response) {
    status_ = status;
    response_ = std::move(response);
  }

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.Bind(mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder>(
        std::move(handle)));
  }

  MockPaintPreviewRecorder(const MockPaintPreviewRecorder&) = delete;
  MockPaintPreviewRecorder& operator=(const MockPaintPreviewRecorder&) = delete;

 private:
  void CheckParams(mojom::PaintPreviewCaptureParamsPtr input_params) {
    // Ignore GUID and File as this is internal information not known by the
    // Keyed Service API.
    EXPECT_EQ(input_params->clip_rect, expected_params_->clip_rect);
    EXPECT_EQ(input_params->is_main_frame, expected_params_->is_main_frame);
  }

  mojom::PaintPreviewCaptureParamsPtr expected_params_;
  mojom::PaintPreviewStatus status_;
  mojom::PaintPreviewCaptureResponsePtr response_;
  mojo::AssociatedReceiver<mojom::PaintPreviewRecorder> binding_{this};
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

  void OverrideInterface(MockPaintPreviewRecorder* service) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        main_rfh()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::PaintPreviewRecorder::Name_,
        base::BindRepeating(&MockPaintPreviewRecorder::BindRequest,
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
      bool capture_links,
      size_t max_per_capture_size,
      uint64_t max_decoded_image_size_bytes) {
    PaintPreviewBaseService::CaptureParams capture_params;
    capture_params.web_contents = web_contents;
    capture_params.root_dir = root_dir;
    capture_params.persistence = persistence;
    capture_params.clip_rect = clip_rect;
    capture_params.capture_links = capture_links;
    capture_params.max_per_capture_size = max_per_capture_size;
    capture_params.max_decoded_image_size_bytes = max_decoded_image_size_bytes;
    return capture_params;
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Instantiate LacrosService for WakeLock support during capturing.
  chromeos::ScopedLacrosServiceTestHelper scoped_lacros_service_test_helper_;
#endif
  std::unique_ptr<SimpleFactoryKey> key_;
  std::unique_ptr<SimpleFactoryKey> rejection_policy_key_;
};

TEST_P(PaintPreviewBaseServiceTest, CaptureMainFrame) {
  MockPaintPreviewRecorder recorder;
  auto params = mojom::PaintPreviewCaptureParams::New();
  params->clip_rect = gfx::Rect(0, 0, 0, 0);
  params->is_main_frame = true;
  params->max_capture_size = 50;
  params->max_decoded_image_size_bytes = 1000;
  recorder.SetExpectedParams(std::move(params));
  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->embedding_token = std::nullopt;
  if (GetParam() == RecordingPersistence::kMemoryBuffer) {
    response->skp.emplace(mojo_base::BigBuffer());
  }
  recorder.SetResponse(mojom::PaintPreviewStatus::kOk, std::move(response));
  OverrideInterface(&recorder);

  auto* service = GetService();
  EXPECT_FALSE(service->IsOffTheRecord());
  auto manager = service->GetFileMixin()->GetFileManager();
  base::FilePath path = CreateDir(
      manager, manager->CreateKey(web_contents()->GetLastCommittedURL()));

  base::RunLoop loop;
  service->CapturePaintPreview(
      CreateCaptureParams(web_contents(), &path, GetParam(),
                          gfx::Rect(0, 0, 0, 0), true, 50, 1000),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             PaintPreviewBaseService::CaptureStatus expected_status,
             const base::FilePath& expected_path,
             PaintPreviewBaseService::CaptureStatus status,
             std::unique_ptr<CaptureResult> result) {
            EXPECT_EQ(status, expected_status);
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
                base::FilePath path = base::FilePath(
                    base::UTF8ToWide(result->proto.root_frame().file_path()));
                base::FilePath name(
                    base::UTF8ToWide(base::StrCat({token.ToString(), ".skp"})));
#else
                base::FilePath path =
                    base::FilePath(result->proto.root_frame().file_path());
                base::FilePath name(base::StrCat({token.ToString(), ".skp"}));
#endif
                EXPECT_EQ(path.DirName(), expected_path);
                EXPECT_EQ(path.BaseName(), name);
              } break;

              case RecordingPersistence::kMemoryBuffer: {
                EXPECT_EQ(result->serialized_skps.size(), 1u);
                EXPECT_TRUE(result->serialized_skps.contains(token));
              } break;

              default:
                NOTREACHED_IN_MIGRATION();
                break;
            }
            std::move(quit_closure).Run();
          },
          loop.QuitClosure(), PaintPreviewBaseService::CaptureStatus::kOk,
          path));
  loop.Run();
}

TEST_P(PaintPreviewBaseServiceTest, CaptureFailed) {
  MockPaintPreviewRecorder recorder;
  auto params = mojom::PaintPreviewCaptureParams::New();
  params->clip_rect = gfx::Rect(0, 0, 0, 0);
  params->is_main_frame = true;
  params->max_capture_size = 0;
  recorder.SetExpectedParams(std::move(params));
  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->embedding_token = std::nullopt;
  recorder.SetResponse(mojom::PaintPreviewStatus::kFailed, std::move(response));
  OverrideInterface(&recorder);

  auto* service = GetService();
  EXPECT_FALSE(service->IsOffTheRecord());
  auto manager = service->GetFileMixin()->GetFileManager();
  base::FilePath path = CreateDir(
      manager, manager->CreateKey(web_contents()->GetLastCommittedURL()));

  base::RunLoop loop;
  service->CapturePaintPreview(
      CreateCaptureParams(web_contents(), &path, GetParam(),
                          gfx::Rect(0, 0, 0, 0), true, 0,
                          std::numeric_limits<uint64_t>::max()),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             PaintPreviewBaseService::CaptureStatus expected_status,
             PaintPreviewBaseService::CaptureStatus status,
             std::unique_ptr<CaptureResult> result) {
            EXPECT_EQ(status, expected_status);
            EXPECT_EQ(result, nullptr);
            std::move(quit_closure).Run();
          },
          loop.QuitClosure(),
          PaintPreviewBaseService::CaptureStatus::kCaptureFailed));
  loop.Run();
}

TEST_P(PaintPreviewBaseServiceTest, CaptureDisallowed) {
  MockPaintPreviewRecorder recorder;
  auto params = mojom::PaintPreviewCaptureParams::New();
  params->clip_rect = gfx::Rect(0, 0, 0, 0);
  params->is_main_frame = true;
  params->max_capture_size = 0;
  recorder.SetExpectedParams(std::move(params));
  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->embedding_token = std::nullopt;
  recorder.SetResponse(mojom::PaintPreviewStatus::kFailed, std::move(response));
  OverrideInterface(&recorder);

  auto* service = GetServiceWithRejectionPolicy();
  EXPECT_FALSE(service->IsOffTheRecord());
  auto manager = service->GetFileMixin()->GetFileManager();
  base::FilePath path = CreateDir(
      manager, manager->CreateKey(web_contents()->GetLastCommittedURL()));

  base::RunLoop loop;
  service->CapturePaintPreview(
      CreateCaptureParams(web_contents(), &path, GetParam(),
                          gfx::Rect(0, 0, 0, 0), true, 0,
                          std::numeric_limits<uint64_t>::max()),
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             PaintPreviewBaseService::CaptureStatus expected_status,
             PaintPreviewBaseService::CaptureStatus status,
             std::unique_ptr<CaptureResult> result) {
            EXPECT_EQ(status, expected_status);
            EXPECT_EQ(result, nullptr);
            std::move(quit_closure).Run();
          },
          loop.QuitClosure(),
          PaintPreviewBaseService::CaptureStatus::kContentUnsupported));
  loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PaintPreviewBaseServiceTest,
                         testing::Values(RecordingPersistence::kFileSystem,
                                         RecordingPersistence::kMemoryBuffer),
                         PersistenceParamToString);

}  // namespace paint_preview
