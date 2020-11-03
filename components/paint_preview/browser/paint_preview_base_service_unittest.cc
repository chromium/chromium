// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_base_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/paint_preview/browser/paint_preview_base_service_test_factory.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

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
      key->GetPath(), kTestFeatureDir,
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
             const base::Optional<base::FilePath>& path) {
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

class PaintPreviewBaseServiceTest : public content::RenderViewHostTestHarness {
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

 private:
  std::unique_ptr<SimpleFactoryKey> key_ = nullptr;
  std::unique_ptr<SimpleFactoryKey> rejection_policy_key_ = nullptr;
};

TEST_F(PaintPreviewBaseServiceTest, CaptureMainFrame) {
  MockPaintPreviewRecorder recorder;
  auto params = mojom::PaintPreviewCaptureParams::New();
  params->clip_rect = gfx::Rect(0, 0, 0, 0);
  params->is_main_frame = true;
  params->max_capture_size = 50;
  recorder.SetExpectedParams(std::move(params));
  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->embedding_token = base::nullopt;
  recorder.SetResponse(mojom::PaintPreviewStatus::kOk, std::move(response));
  OverrideInterface(&recorder);

  auto* service = GetService();
  EXPECT_FALSE(service->IsOffTheRecord());
  auto manager = service->GetFileManager();
  base::FilePath path = CreateDir(
      manager, manager->CreateKey(web_contents()->GetLastCommittedURL()));

  base::RunLoop loop;
  service->CapturePaintPreview(
      web_contents(), path, gfx::Rect(0, 0, 0, 0), true, 50,
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
                result->proto.root_frame().embedding_token_low());
#if defined(OS_WIN)
            base::FilePath path = base::FilePath(
                base::UTF8ToUTF16(result->proto.root_frame().file_path()));
            base::FilePath name(
                base::UTF8ToUTF16(base::StrCat({token.ToString(), ".skp"})));
#else
            base::FilePath path =
                base::FilePath(result->proto.root_frame().file_path());
            base::FilePath name(base::StrCat({token.ToString(), ".skp"}));
#endif
            EXPECT_EQ(path.DirName(), expected_path);
            LOG(ERROR) << expected_path;
            EXPECT_EQ(path.BaseName(), name);
            std::move(quit_closure).Run();
          },
          loop.QuitClosure(), PaintPreviewBaseService::CaptureStatus::kOk,
          path));
  loop.Run();
}

TEST_F(PaintPreviewBaseServiceTest, CaptureFailed) {
  MockPaintPreviewRecorder recorder;
  auto params = mojom::PaintPreviewCaptureParams::New();
  params->clip_rect = gfx::Rect(0, 0, 0, 0);
  params->is_main_frame = true;
  params->max_capture_size = 0;
  recorder.SetExpectedParams(std::move(params));
  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->embedding_token = base::nullopt;
  recorder.SetResponse(mojom::PaintPreviewStatus::kFailed, std::move(response));
  OverrideInterface(&recorder);

  auto* service = GetService();
  EXPECT_FALSE(service->IsOffTheRecord());
  auto manager = service->GetFileManager();
  base::FilePath path = CreateDir(
      manager, manager->CreateKey(web_contents()->GetLastCommittedURL()));

  base::RunLoop loop;
  service->CapturePaintPreview(
      web_contents(), path, gfx::Rect(0, 0, 0, 0), true, 0,
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

TEST_F(PaintPreviewBaseServiceTest, CaptureDisallowed) {
  MockPaintPreviewRecorder recorder;
  auto params = mojom::PaintPreviewCaptureParams::New();
  params->clip_rect = gfx::Rect(0, 0, 0, 0);
  params->is_main_frame = true;
  params->max_capture_size = 0;
  recorder.SetExpectedParams(std::move(params));
  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->embedding_token = base::nullopt;
  recorder.SetResponse(mojom::PaintPreviewStatus::kFailed, std::move(response));
  OverrideInterface(&recorder);

  auto* service = GetServiceWithRejectionPolicy();
  EXPECT_FALSE(service->IsOffTheRecord());
  auto manager = service->GetFileManager();
  base::FilePath path = CreateDir(
      manager, manager->CreateKey(web_contents()->GetLastCommittedURL()));

  base::RunLoop loop;
  service->CapturePaintPreview(
      web_contents(), path, gfx::Rect(0, 0, 0, 0), true, 0,
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

}  // namespace paint_preview
