// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_client.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/paint_preview/common/capture_result.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-forward.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-shared.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/test_utils.h"
#include "components/paint_preview/common/version.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace paint_preview {

namespace {

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
      if (closure_)
        std::move(closure_).Run();
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

  void SetResponseAction(base::OnceClosure closure) {
    closure_ = std::move(closure);
  }

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.Bind(mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder>(
        std::move(handle)));
  }

 private:
  void CheckParams(mojom::PaintPreviewCaptureParamsPtr input_params) {
    EXPECT_EQ(input_params->guid, expected_params_->guid);
    EXPECT_EQ(input_params->clip_rect, expected_params_->clip_rect);
    EXPECT_EQ(input_params->is_main_frame, expected_params_->is_main_frame);
    if (expected_params_->is_main_frame) {
      EXPECT_FALSE(input_params->clip_rect_is_hint);
    }
  }

  base::OnceClosure closure_;
  mojom::PaintPreviewCaptureParamsPtr expected_params_;
  mojom::PaintPreviewStatus status_;
  mojom::PaintPreviewCaptureResponsePtr response_;
  mojo::AssociatedReceiver<mojom::PaintPreviewRecorder> binding_{this};

  MockPaintPreviewRecorder(const MockPaintPreviewRecorder&) = delete;
  MockPaintPreviewRecorder& operator=(const MockPaintPreviewRecorder&) = delete;
};

// Convert |params| to the mojo::PaintPreviewServiceParams format. NOTE: this
// does not set the file parameter as the file is created in the client
// internals and should be treated as an opaque file (with an unknown path) in
// the render frame's service.
mojom::PaintPreviewCaptureParamsPtr ToMojoParams(
    PaintPreviewClient::PaintPreviewParams params) {
  mojom::PaintPreviewCaptureParamsPtr params_ptr =
      mojom::PaintPreviewCaptureParams::New();
  params_ptr->persistence = params.persistence;
  params_ptr->guid = params.inner.document_guid;
  params_ptr->is_main_frame = params.inner.is_main_frame;
  params_ptr->clip_rect = params.inner.clip_rect;
  params_ptr->skip_accelerated_content = params.inner.skip_accelerated_content;
  return params_ptr;
}

}  // namespace

class PaintPreviewClientRenderViewHostTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<RecordingPersistence> {
 public:
  PaintPreviewClientRenderViewHostTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    RenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://www.chromium.org"));
  }

  mojo::StructPtr<mojom::PaintPreviewCaptureResponse>
  NewMockPaintPreviewCaptureResponse() {
    auto response = mojom::PaintPreviewCaptureResponse::New();
    if (GetParam() == RecordingPersistence::kMemoryBuffer) {
      response->skp = {mojo_base::BigBuffer()};
    }
    return response;
  }

  void OverrideInterface(MockPaintPreviewRecorder* service) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::PaintPreviewRecorder::Name_,
        base::BindRepeating(&MockPaintPreviewRecorder::BindRequest,
                            base::Unretained(service)));
  }

  base::ScopedTempDir temp_dir_;

 private:
  PaintPreviewClientRenderViewHostTest(
      const PaintPreviewClientRenderViewHostTest&) = delete;
  PaintPreviewClientRenderViewHostTest& operator=(
      const PaintPreviewClientRenderViewHostTest&) = delete;
};

TEST_P(PaintPreviewClientRenderViewHostTest, CaptureMainFrameMock) {
  PaintPreviewClient::PaintPreviewParams params(GetParam());
  params.root_dir = temp_dir_.GetPath();
  params.inner.is_main_frame = true;

  content::RenderFrameHost* rfh = main_rfh();
  GURL expected_url = rfh->GetLastCommittedURL();

  auto response = NewMockPaintPreviewCaptureResponse();
  response->embedding_token = std::nullopt;
  response->scroll_offsets = gfx::Point(5, 10);
  response->frame_offsets = gfx::Point(20, 30);

  PaintPreviewProto expected_proto;
  auto* metadata = expected_proto.mutable_metadata();
  metadata->set_url(expected_url.spec());
  metadata->set_version(kPaintPreviewVersion);
  auto* chromeVersion = metadata->mutable_chrome_version();
  const auto& current_chrome_version = version_info::GetVersion();
  chromeVersion->set_major(current_chrome_version.components()[0]);
  chromeVersion->set_minor(current_chrome_version.components()[1]);
  chromeVersion->set_build(current_chrome_version.components()[2]);
  chromeVersion->set_patch(current_chrome_version.components()[3]);
  PaintPreviewFrameProto* main_frame = expected_proto.mutable_root_frame();
  main_frame->set_is_main_frame(true);
  main_frame->set_scroll_offset_x(5);
  main_frame->set_scroll_offset_y(10);
  main_frame->set_frame_offset_x(20);
  main_frame->set_frame_offset_y(30);

  base::RunLoop loop;
  auto callback = base::BindOnce(
      [](base::RepeatingClosure quit, base::UnguessableToken expected_guid,
         base::FilePath temp_dir, PaintPreviewProto expected_proto,
         base::UnguessableToken returned_guid, mojom::PaintPreviewStatus status,
         std::unique_ptr<CaptureResult> result) {
        EXPECT_EQ(returned_guid, expected_guid);
        EXPECT_EQ(status, mojom::PaintPreviewStatus::kOk);

        auto token = base::UnguessableToken::Deserialize(
                         result->proto.root_frame().embedding_token_high(),
                         result->proto.root_frame().embedding_token_low())
                         .value();
        EXPECT_NE(token, base::UnguessableToken::Null());

        // The token for the main frame is set internally since the render frame
        // host won't have one. To simplify the proto comparison using
        // EqualsProto copy the generated one into |expected_proto|.
        PaintPreviewFrameProto* main_frame =
            expected_proto.mutable_root_frame();
        main_frame->set_embedding_token_low(token.GetLowForSerialization());
        main_frame->set_embedding_token_high(token.GetHighForSerialization());
        if (GetParam() == RecordingPersistence::kFileSystem) {
          main_frame->set_file_path(
              temp_dir.AppendASCII(base::StrCat({token.ToString(), ".skp"}))
                  .AsUTF8Unsafe());
        }

        EXPECT_THAT(result->proto, EqualsProto(expected_proto));

        switch (GetParam()) {
          case RecordingPersistence::kFileSystem: {
            base::ScopedAllowBlockingForTesting scope;
#if BUILDFLAG(IS_WIN)
            base::FilePath path = base::FilePath(
                base::UTF8ToWide(result->proto.root_frame().file_path()));
#else
            base::FilePath path =
                base::FilePath(result->proto.root_frame().file_path());
#endif
            EXPECT_TRUE(base::PathExists(path));
          } break;

          case RecordingPersistence::kMemoryBuffer: {
            EXPECT_EQ(result->serialized_skps.size(), 1u);
            EXPECT_TRUE(result->serialized_skps.contains(token));
          } break;

          default:
            NOTREACHED_IN_MIGRATION();
            break;
        }

        quit.Run();
      },
      loop.QuitClosure(), params.inner.document_guid, temp_dir_.GetPath(),
      expected_proto);
  MockPaintPreviewRecorder service;
  service.SetExpectedParams(ToMojoParams(params));
  service.SetResponse(mojom::PaintPreviewStatus::kOk, std::move(response));
  OverrideInterface(&service);
  PaintPreviewClient::CreateForWebContents(web_contents());
  auto* client = PaintPreviewClient::FromWebContents(web_contents());
  ASSERT_NE(client, nullptr);
  client->CapturePaintPreview(params, rfh, std::move(callback));
  loop.Run();
}

TEST_P(PaintPreviewClientRenderViewHostTest, CaptureFailureMock) {
  PaintPreviewClient::PaintPreviewParams params(GetParam());
  params.root_dir = temp_dir_.GetPath();
  params.inner.is_main_frame = true;

  auto response = NewMockPaintPreviewCaptureResponse();
  response->skp = {mojo_base::BigBuffer()};

  base::RunLoop loop;
  auto callback = base::BindOnce(
      [](base::RepeatingClosure quit, base::UnguessableToken expected_guid,
         base::UnguessableToken returned_guid, mojom::PaintPreviewStatus status,
         std::unique_ptr<CaptureResult> result) {
        EXPECT_EQ(returned_guid, expected_guid);
        EXPECT_EQ(status, mojom::PaintPreviewStatus::kFailed);
        quit.Run();
      },
      loop.QuitClosure(), params.inner.document_guid);
  MockPaintPreviewRecorder recorder;
  recorder.SetExpectedParams(ToMojoParams(params));
  recorder.SetResponse(mojom::PaintPreviewStatus::kCaptureFailed,
                       std::move(response));
  OverrideInterface(&recorder);
  PaintPreviewClient::CreateForWebContents(web_contents());
  auto* client = PaintPreviewClient::FromWebContents(web_contents());
  ASSERT_NE(client, nullptr);
  client->CapturePaintPreview(params, main_rfh(), std::move(callback));
  loop.Run();
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_.GetPath()));
}

TEST_P(PaintPreviewClientRenderViewHostTest, RenderFrameDeletedNotCapturing) {
  // Test that a deleting a render frame doesn't cause any problems if not
  // capturing.
  PaintPreviewClient::CreateForWebContents(web_contents());
  auto* client = PaintPreviewClient::FromWebContents(web_contents());
  ASSERT_NE(client, nullptr);
  client->RenderFrameDeleted(main_rfh());
}

TEST_P(PaintPreviewClientRenderViewHostTest, RenderFrameDeletedDuringCapture) {
  PaintPreviewClient::PaintPreviewParams params(GetParam());
  params.root_dir = temp_dir_.GetPath();
  params.inner.is_main_frame = true;
  params.inner.skip_accelerated_content = true;

  content::RenderFrameHost* rfh = main_rfh();

  auto response = NewMockPaintPreviewCaptureResponse();
  response->embedding_token = std::nullopt;

  base::RunLoop loop;
  auto callback = base::BindOnce(
      [](base::RepeatingClosure quit, base::UnguessableToken returned_guid,
         mojom::PaintPreviewStatus status,
         std::unique_ptr<CaptureResult> result) {
        EXPECT_EQ(status, mojom::PaintPreviewStatus::kFailed);
        EXPECT_EQ(result, nullptr);
        quit.Run();
      },
      loop.QuitClosure());
  MockPaintPreviewRecorder service;
  service.SetExpectedParams(ToMojoParams(params));
  service.SetResponse(mojom::PaintPreviewStatus::kOk, std::move(response));
  OverrideInterface(&service);
  PaintPreviewClient::CreateForWebContents(web_contents());
  auto* client = PaintPreviewClient::FromWebContents(web_contents());
  ASSERT_NE(client, nullptr);
  service.SetResponseAction(
      base::BindOnce(&PaintPreviewClient::RenderFrameDeleted,
                     base::Unretained(client), base::Unretained(rfh)));
  client->CapturePaintPreview(params, rfh, std::move(callback));
  loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PaintPreviewClientRenderViewHostTest,
                         testing::Values(RecordingPersistence::kFileSystem,
                                         RecordingPersistence::kMemoryBuffer),
                         PersistenceParamToString);

}  // namespace paint_preview
