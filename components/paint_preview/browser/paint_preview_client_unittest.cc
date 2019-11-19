// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_client.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
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
  }

  base::OnceClosure closure_;
  mojom::PaintPreviewCaptureParamsPtr expected_params_;
  mojom::PaintPreviewStatus status_;
  mojom::PaintPreviewCaptureResponsePtr response_;
  mojo::AssociatedReceiver<mojom::PaintPreviewRecorder> binding_{this};

  MockPaintPreviewRecorder(const MockPaintPreviewRecorder&) = delete;
  MockPaintPreviewRecorder& operator=(const MockPaintPreviewRecorder&) = delete;
};

// Returns the GUID corresponding to |rfh|.
uint64_t FrameGuid(content::RenderFrameHost* rfh) {
  return static_cast<uint64_t>(rfh->GetProcess()->GetID()) << 32 |
         rfh->GetRoutingID();
}

// Convert |params| to the mojo::PaintPreviewServiceParams format. NOTE: this
// does not set the file parameter as the file is created in the client
// internals and should be treated as an opaque file (with an unknown path) in
// the render frame's service.
mojom::PaintPreviewCaptureParamsPtr ToMojoParams(
    PaintPreviewClient::PaintPreviewParams params) {
  mojom::PaintPreviewCaptureParamsPtr params_ptr =
      mojom::PaintPreviewCaptureParams::New();
  params_ptr->guid = params.document_guid;
  params_ptr->is_main_frame = params.is_main_frame;
  params_ptr->clip_rect = params.clip_rect;
  return params_ptr;
}

}  // namespace

class PaintPreviewClientRenderViewHostTest
    : public content::RenderViewHostTestHarness {
 public:
  PaintPreviewClientRenderViewHostTest() {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    RenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
  }

  void OverrideInterface(MockPaintPreviewRecorder* service) {
    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces();
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

TEST_F(PaintPreviewClientRenderViewHostTest, CaptureMainFrameMock) {
  PaintPreviewClient::PaintPreviewParams params;
  params.document_guid = base::UnguessableToken::Create();
  params.root_dir = temp_dir_.GetPath();
  params.is_main_frame = true;

  content::RenderFrameHost* rfh = main_rfh();
  uint64_t frame_guid = FrameGuid(rfh);

  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->id = rfh->GetRoutingID();

  PaintPreviewProto expected_proto;
  PaintPreviewFrameProto* main_frame = expected_proto.mutable_root_frame();
  main_frame->set_is_main_frame(true);
  main_frame->set_id(frame_guid);
  main_frame->set_file_path(
      temp_dir_.GetPath()
          .AppendASCII(
              base::StrCat({base::NumberToString(main_frame->id()), ".skp"}))
          .AsUTF8Unsafe());

  base::RunLoop loop;
  auto callback = base::BindOnce(
      [](base::RepeatingClosure quit, base::UnguessableToken expected_guid,
         PaintPreviewProto expected_proto, base::UnguessableToken returned_guid,
         mojom::PaintPreviewStatus status,
         std::unique_ptr<PaintPreviewProto> proto) {
        EXPECT_EQ(returned_guid, expected_guid);
        EXPECT_EQ(status, mojom::PaintPreviewStatus::kOk);

        EXPECT_THAT(*proto, EqualsProto(expected_proto));
        {
          base::ScopedAllowBlockingForTesting scope;
#if defined(OS_WIN)
          base::FilePath path = base::FilePath(
              base::UTF8ToUTF16(proto->root_frame().file_path()));
#else
          base::FilePath path = base::FilePath(proto->root_frame().file_path());
#endif
          EXPECT_TRUE(base::PathExists(path));
        }
        quit.Run();
      },
      loop.QuitClosure(), params.document_guid, expected_proto);
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

TEST_F(PaintPreviewClientRenderViewHostTest, CaptureFailureMock) {
  PaintPreviewClient::PaintPreviewParams params;
  params.document_guid = base::UnguessableToken::Create();
  params.root_dir = temp_dir_.GetPath();
  params.is_main_frame = true;

  auto response = mojom::PaintPreviewCaptureResponse::New();

  base::RunLoop loop;
  auto callback = base::BindOnce(
      [](base::RepeatingClosure quit, base::UnguessableToken expected_guid,
         base::UnguessableToken returned_guid, mojom::PaintPreviewStatus status,
         std::unique_ptr<PaintPreviewProto> proto) {
        EXPECT_EQ(returned_guid, expected_guid);
        EXPECT_EQ(status, mojom::PaintPreviewStatus::kFailed);
        quit.Run();
      },
      loop.QuitClosure(), params.document_guid);
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
}

TEST_F(PaintPreviewClientRenderViewHostTest, RenderFrameDeletedNotCapturing) {
  // Test that a deleting a render frame doesn't cause any problems if not
  // capturing.
  PaintPreviewClient::CreateForWebContents(web_contents());
  auto* client = PaintPreviewClient::FromWebContents(web_contents());
  ASSERT_NE(client, nullptr);
  client->RenderFrameDeleted(main_rfh());
}

TEST_F(PaintPreviewClientRenderViewHostTest, RenderFrameDeletedDuringCapture) {
  PaintPreviewClient::PaintPreviewParams params;
  params.document_guid = base::UnguessableToken::Create();
  params.root_dir = temp_dir_.GetPath();
  params.is_main_frame = true;

  content::RenderFrameHost* rfh = main_rfh();

  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->id = rfh->GetRoutingID();

  base::RunLoop loop;
  auto callback = base::BindOnce(
      [](base::RepeatingClosure quit, base::UnguessableToken returned_guid,
         mojom::PaintPreviewStatus status,
         std::unique_ptr<PaintPreviewProto> proto) {
        EXPECT_EQ(status, mojom::PaintPreviewStatus::kFailed);
        EXPECT_EQ(proto, nullptr);
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

}  // namespace paint_preview
