// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_client.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/paint_preview/common/capture_result.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/mock_paint_preview_recorder.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-data-view.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-forward.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/redaction_params.h"
#include "components/paint_preview/common/serialized_recording.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"

namespace paint_preview {

using base::test::EqualsProto;
using testing::AnyOf;
using testing::Key;
using testing::UnorderedElementsAre;

using CaptureResultFuture =
    base::test::TestFuture<base::UnguessableToken,
                           mojom::PaintPreviewStatus,
                           std::unique_ptr<CaptureResult>>;

namespace {

const std::string_view kMainFrameUrl = "https://www.chromium.org";
const std::string_view kOtherOriginUrl = "https://chromium.org";

// Convert |params| to the mojo::PaintPreviewServiceParams format. NOTE: this
// does not set the file parameter as the file is created in the client
// internals and should be treated as an opaque file (with an unknown path) in
// the render frame's service.
mojom::PaintPreviewCaptureParamsPtr ToMojoParams(
    const PaintPreviewClient::PaintPreviewParams& params) {
  mojom::PaintPreviewCaptureParamsPtr params_ptr =
      mojom::PaintPreviewCaptureParams::New();
  params_ptr->persistence = params.persistence;
  params_ptr->guid = params.inner.get_document_guid();
  params_ptr->is_main_frame = params.inner.is_main_frame;
  params_ptr->geometry_metadata_params = mojom::GeometryMetadataParams::New();
  params_ptr->geometry_metadata_params->clip_rect = params.inner.clip_rect;
  params_ptr->skip_accelerated_content = params.inner.skip_accelerated_content;
  return params_ptr;
}

base::FilePath ParseStringFilePath(std::string_view raw_path) {
  return base::FilePath(
#if BUILDFLAG(IS_WIN)
      base::UTF8ToWide(raw_path)
#else
      raw_path
#endif
  );
}

void VerifyFilePath(std::string_view raw_path,
                    const base::Location& location = FROM_HERE) {
  base::ScopedAllowBlockingForTesting scope;
  EXPECT_TRUE(base::PathExists(ParseStringFilePath(raw_path)))
      << "Expected at " << location.ToString();
}

base::UnguessableToken DeserializeFrameToken(
    const PaintPreviewFrameProto& frame) {
  auto token = base::UnguessableToken::Deserialize(frame.embedding_token_high(),
                                                   frame.embedding_token_low())
                   .value();
  CHECK_NE(token, base::UnguessableToken::Null());
  return token;
}

sk_sp<SkPicture> ReadPictureFromFile(std::string_view raw_path) {
  base::ScopedAllowBlockingForTesting scope;
  FileRStream rstream(
      base::File(ParseStringFilePath(raw_path),
                 base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ));
  return SkPicture::MakeFromStream(&rstream, nullptr);
}

sk_sp<SkPicture> ReadPictureFromBuffer(mojo_base::BigBuffer buffer) {
  base::ScopedAllowBlockingForTesting scope;
  SkMemoryStream stream(buffer.data(), buffer.size(),
                        /*copyData=*/false);
  return SkPicture::MakeFromStream(&stream, nullptr);
}

SkBitmap MakeBitmapFromPicture(sk_sp<SkPicture> pic) {
  SkBitmap bitmap;
  CHECK(bitmap.tryAllocN32Pixels(pic->cullRect().width(),
                                 pic->cullRect().height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.drawPicture(pic);

  return bitmap;
}

}  // namespace

class PaintPreviewClientRenderViewHostTestBase
    : public content::RenderViewHostTestHarness {
 public:
  PaintPreviewClientRenderViewHostTestBase() = default;

  PaintPreviewClientRenderViewHostTestBase(
      const PaintPreviewClientRenderViewHostTestBase&) = delete;
  PaintPreviewClientRenderViewHostTestBase& operator=(
      const PaintPreviewClientRenderViewHostTestBase&) = delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    RenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL(kMainFrameUrl));
  }

  void TearDown() override {
#if BUILDFLAG(IS_POSIX)
    base::SetPosixFilePermissions(temp_dir_.GetPath(), 0777);
#endif
    content::RenderViewHostTestHarness::TearDown();
  }

  void OverrideInterface(content::RenderFrameHost* rfh,
                         MockPaintPreviewRecorder* service) {
    ASSERT_TRUE(rfh);
    blink::AssociatedInterfaceProvider* remote_interfaces =
        rfh->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        mojom::PaintPreviewRecorder::Name_,
        base::BindRepeating(&MockPaintPreviewRecorder::BindRequest,
                            base::Unretained(service)));
  }

  content::RenderFrameHost* AddChildRFH(content::RenderFrameHost* parent,
                                        std::string_view origin) {
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChild("");
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, GURL(origin));
    return result;
  }

  void SimulateNavigation(content::RenderFrameHost** rfh, const GURL& url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, *rfh);
    navigation_simulator->Commit();
    *rfh = navigation_simulator->GetFinalRenderFrameHost();
  }

  base::ScopedTempDir temp_dir_;

 private:
};

class PaintPreviewClientRenderViewHostTest
    : public PaintPreviewClientRenderViewHostTestBase,
      public testing::WithParamInterface<RecordingPersistence> {
 public:
  mojo::StructPtr<mojom::PaintPreviewCaptureResponse>
  NewMockPaintPreviewCaptureResponse() {
    auto response = mojom::PaintPreviewCaptureResponse::New();
    response->geometry_metadata = mojom::GeometryMetadataResponse::New();
    if (GetParam() == RecordingPersistence::kMemoryBuffer) {
      response->skp = {mojo_base::BigBuffer()};
    }
    return response;
  }

 private:
};

TEST_P(PaintPreviewClientRenderViewHostTest, CaptureMainFrameMock) {
  PaintPreviewClient::PaintPreviewParams params(GetParam());
  params.root_dir = temp_dir_.GetPath();
  params.inner.is_main_frame = true;

  content::RenderFrameHost* rfh = main_rfh();
  GURL expected_url = rfh->GetLastCommittedURL();

  auto response = NewMockPaintPreviewCaptureResponse();
  response->embedding_token = std::nullopt;
  response->geometry_metadata->scroll_offsets = gfx::Point(5, 10);
  response->geometry_metadata->frame_offsets = gfx::Point(20, 30);

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

  MockPaintPreviewRecorder service;
  service.SetExpectedParams(ToMojoParams(params));
  service.SetResponse(std::move(response));
  OverrideInterface(rfh, &service);
  PaintPreviewClient::CreateForWebContents(web_contents());
  auto* client = PaintPreviewClient::FromWebContents(web_contents());
  ASSERT_NE(client, nullptr);
  CaptureResultFuture main_capture_future;
  client->CapturePaintPreview(params.Clone(), rfh,
                              main_capture_future.GetCallback());

  auto [returned_guid, status, result] = main_capture_future.Take();
  EXPECT_EQ(returned_guid, params.inner.get_document_guid());
  EXPECT_EQ(status, mojom::PaintPreviewStatus::kOk);

  auto token = DeserializeFrameToken(result->proto.root_frame());

  // The token for the main frame is set internally since the render frame
  // host won't have one. To simplify the proto comparison using
  // EqualsProto copy the generated one into |expected_proto|.
  main_frame->set_embedding_token_low(token.GetLowForSerialization());
  main_frame->set_embedding_token_high(token.GetHighForSerialization());
  if (GetParam() == RecordingPersistence::kFileSystem) {
    main_frame->set_file_path(
        temp_dir_.GetPath()
            .AppendASCII(base::StrCat({token.ToString(), ".skp"}))
            .AsUTF8Unsafe());
  }

  EXPECT_THAT(result->proto, EqualsProto(expected_proto));

  switch (GetParam()) {
    case RecordingPersistence::kFileSystem:
      VerifyFilePath(result->proto.root_frame().file_path());
      break;

    case RecordingPersistence::kMemoryBuffer: {
      EXPECT_EQ(result->serialized_skps.size(), 1u);
      EXPECT_TRUE(result->serialized_skps.contains(token));
    } break;

    default:
      NOTREACHED();
  }
}

TEST_P(PaintPreviewClientRenderViewHostTest, CaptureFailureMock) {
  PaintPreviewClient::PaintPreviewParams params(GetParam());
  params.root_dir = temp_dir_.GetPath();
  params.inner.is_main_frame = true;

  MockPaintPreviewRecorder recorder;
  recorder.SetExpectedParams(ToMojoParams(params));
  recorder.SetResponse(
      base::unexpected(mojom::PaintPreviewStatus::kCaptureFailed));
  OverrideInterface(main_rfh(), &recorder);
  PaintPreviewClient::CreateForWebContents(web_contents());
  auto* client = PaintPreviewClient::FromWebContents(web_contents());
  ASSERT_NE(client, nullptr);
  CaptureResultFuture main_capture_future;
  client->CapturePaintPreview(params.Clone(), main_rfh(),
                              main_capture_future.GetCallback());

  auto [returned_guid, status, result] = main_capture_future.Take();
  EXPECT_EQ(returned_guid, params.inner.get_document_guid());
  EXPECT_EQ(status, mojom::PaintPreviewStatus::kFailed);
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_.GetPath()));
}

// We can only mutate file permissions on POSIX systems.
#if BUILDFLAG(IS_POSIX)
TEST_F(PaintPreviewClientRenderViewHostTestBase, SubframeFileCreationFails) {
  auto* subframe = AddChildRFH(main_rfh(), "https://example.test");
  ASSERT_TRUE(subframe);

  PaintPreviewClient::PaintPreviewParams params(
      RecordingPersistence::kFileSystem);
  params.root_dir = temp_dir_.GetPath();
  params.inner.is_main_frame = true;

  MockPaintPreviewRecorder recorder;
  recorder.SetExpectedParams(ToMojoParams(params));
  recorder.SetResponse(
      base::unexpected(mojom::PaintPreviewStatus::kCaptureFailed));
  OverrideInterface(main_rfh(), &recorder);

  PaintPreviewClient::CreateForWebContents(web_contents());
  auto* client = PaintPreviewClient::FromWebContents(web_contents());
  ASSERT_NE(client, nullptr);

  base::test::TestFuture<void> recorder_received_request_future;
  recorder.SetReceivedRequestClosure(
      recorder_received_request_future.GetCallback());

  CaptureResultFuture main_capture_future;
  client->CapturePaintPreview(params.Clone(), main_rfh(),
                              main_capture_future.GetCallback());

  ASSERT_TRUE(recorder_received_request_future.Wait());
  // Before sending the main frame response, make the directory read-only to
  // cause a file creation error on subsequent writes, and issue a reentrant
  // call for a subframe, to see if the main frame's control flow mistakenly
  // invokes the callback even after the subframe failed the whole capture.

  base::SetPosixFilePermissions(temp_dir_.GetPath(), 0555);

  client->CaptureSubframePaintPreview(params.inner.get_document_guid(),
                                      gfx::Rect(), subframe);

  recorder.SendResponse();

  auto [returned_guid, status, result] = main_capture_future.Take();
  EXPECT_EQ(returned_guid, params.inner.get_document_guid());
  // The precise status from the client depends on thread scheduling.
  EXPECT_THAT(status, AnyOf(mojom::PaintPreviewStatus::kFileCreationError,
                            mojom::PaintPreviewStatus::kFailed));
  EXPECT_EQ(result, nullptr);
}
#endif

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
  response->geometry_metadata = mojom::GeometryMetadataResponse::New();
  response->embedding_token = std::nullopt;

  MockPaintPreviewRecorder service;
  service.SetExpectedParams(ToMojoParams(params));
  service.SetResponse(std::move(response));
  OverrideInterface(rfh, &service);
  PaintPreviewClient::CreateForWebContents(web_contents());
  auto* client = PaintPreviewClient::FromWebContents(web_contents());
  ASSERT_NE(client, nullptr);

  base::test::TestFuture<void> service_received_request_future;
  service.SetReceivedRequestClosure(
      service_received_request_future.GetCallback());

  CaptureResultFuture main_capture_future;
  client->CapturePaintPreview(params.Clone(), rfh,
                              main_capture_future.GetCallback());

  ASSERT_TRUE(service_received_request_future.Wait());
  client->RenderFrameDeleted(rfh);
  service.SendResponse();

  auto [guid, status, result] = main_capture_future.Take();
  EXPECT_EQ(status, mojom::PaintPreviewStatus::kFailed);
  EXPECT_EQ(result, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PaintPreviewClientRenderViewHostTest,
    testing::Values(RecordingPersistence::kFileSystem,
                    RecordingPersistence::kMemoryBuffer),
    [](const testing::TestParamInfo<RecordingPersistence>& info) {
      return std::string(PersistenceToString(info.param));
    });

enum class ResponseOrdering { kMainFrameThenSubframe, kSubframeThenMainFrame };

std::string_view OrderingToString(ResponseOrdering ordering) {
  switch (ordering) {
    case ResponseOrdering::kMainFrameThenSubframe:
      return "kMainFrameThenSubframe";
    case ResponseOrdering::kSubframeThenMainFrame:
      return "kSubframeThenMainFrame";
  }
  NOTREACHED();
}

class PaintPreviewClientRenderViewHostResponseOrderingTest
    : public PaintPreviewClientRenderViewHostTestBase,
      public testing::WithParamInterface<
          std::tuple<RecordingPersistence, ResponseOrdering>> {
 public:
  mojo::StructPtr<mojom::PaintPreviewCaptureResponse>
  NewMockPaintPreviewCaptureResponse() {
    auto response = mojom::PaintPreviewCaptureResponse::New();
    response->geometry_metadata = mojom::GeometryMetadataResponse::New();
    if (persistence() == RecordingPersistence::kMemoryBuffer) {
      response->skp = {mojo_base::BigBuffer()};
    }
    return response;
  }

  RecordingPersistence persistence() const { return std::get<0>(GetParam()); }

  ResponseOrdering ordering() const { return std::get<1>(GetParam()); }

 private:
};

TEST_P(PaintPreviewClientRenderViewHostResponseOrderingTest,
       CaptureMainFrameWithSubframe) {
  content::RenderFrameHost* rfh = main_rfh();
  content::RenderFrameHost* subframe =
      AddChildRFH(rfh, "https://www.chromium.org");

  GURL expected_url = rfh->GetLastCommittedURL();

  PaintPreviewClient::PaintPreviewParams main_frame_params(persistence());
  main_frame_params.root_dir = temp_dir_.GetPath();
  main_frame_params.inner.is_main_frame = true;

  auto main_frame_response = NewMockPaintPreviewCaptureResponse();
  main_frame_response->embedding_token = std::nullopt;
  main_frame_response->geometry_metadata->scroll_offsets = gfx::Point(5, 10);
  main_frame_response->geometry_metadata->frame_offsets = gfx::Point(20, 30);

  auto subframe_response = NewMockPaintPreviewCaptureResponse();
  subframe_response->embedding_token = std::nullopt;
  subframe_response->geometry_metadata->scroll_offsets = gfx::Point(5, 10);
  subframe_response->geometry_metadata->frame_offsets = gfx::Point(20, 30);

  MockPaintPreviewRecorder main_frame_recorder;
  main_frame_recorder.SetExpectedParams(ToMojoParams(main_frame_params));
  main_frame_recorder.SetResponse(std::move(main_frame_response));
  base::test::TestFuture<void> main_frame_req;
  main_frame_recorder.SetReceivedRequestClosure(main_frame_req.GetCallback());
  OverrideInterface(rfh, &main_frame_recorder);

  const gfx::Rect subframe_rect(0, 0, 100, 200);
  PaintPreviewClient::PaintPreviewParams expected_subframe_params =
      PaintPreviewClient::PaintPreviewParams::CreateForTesting(
          persistence(), main_frame_params.inner.get_document_guid());
  expected_subframe_params.root_dir = temp_dir_.GetPath();
  expected_subframe_params.inner.is_main_frame = false;
  expected_subframe_params.inner.clip_rect = subframe_rect;
  MockPaintPreviewRecorder subframe_recorder;
  subframe_recorder.SetExpectedParams(ToMojoParams(expected_subframe_params));
  subframe_recorder.SetResponse(std::move(subframe_response));
  base::test::TestFuture<void> subframe_req;
  subframe_recorder.SetReceivedRequestClosure(subframe_req.GetCallback());
  OverrideInterface(subframe, &subframe_recorder);

  PaintPreviewClient::CreateForWebContents(web_contents());
  auto* client = PaintPreviewClient::FromWebContents(web_contents());
  ASSERT_NE(client, nullptr);

  CaptureResultFuture main_capture_future;
  client->CapturePaintPreview(main_frame_params.Clone(), rfh,
                              main_capture_future.GetCallback());
  ASSERT_TRUE(main_frame_req.Wait());

  client->CaptureSubframePaintPreview(
      main_frame_params.inner.get_document_guid(), subframe_rect, subframe);
  ASSERT_TRUE(subframe_req.Wait());

  switch (ordering()) {
    case ResponseOrdering::kMainFrameThenSubframe:
      main_frame_recorder.SendResponse();
      subframe_recorder.SendResponse();
      break;
    case ResponseOrdering::kSubframeThenMainFrame:
      subframe_recorder.SendResponse();
      main_frame_recorder.SendResponse();
      break;
  }

  auto [main_guid, main_status, result] = main_capture_future.Take();
  EXPECT_EQ(main_guid, main_frame_params.inner.get_document_guid());
  EXPECT_EQ(main_status, mojom::PaintPreviewStatus::kOk);

  auto token = DeserializeFrameToken(result->proto.root_frame());

  switch (persistence()) {
    case RecordingPersistence::kFileSystem:
      VerifyFilePath(result->proto.root_frame().file_path());
      ASSERT_EQ(result->proto.subframes().size(), 1);
      VerifyFilePath(result->proto.subframes()[0].file_path());
      break;

    case RecordingPersistence::kMemoryBuffer:
      EXPECT_EQ(result->serialized_skps.size(), 2u);
      EXPECT_TRUE(result->serialized_skps.contains(token));
      break;

    default:
      NOTREACHED();
  }
}

TEST_P(PaintPreviewClientRenderViewHostResponseOrderingTest,
       CaptureMainFrameWithSubframe_RedactedIframe) {
  content::RenderFrameHost* rfh = main_rfh();
  content::RenderFrameHost* subframe =
      AddChildRFH(rfh, "https://www.chromium.org");

  GURL expected_url = rfh->GetLastCommittedURL();

  PaintPreviewClient::PaintPreviewParams main_frame_params(persistence());
  main_frame_params.root_dir = temp_dir_.GetPath();
  main_frame_params.inner.is_main_frame = true;
  main_frame_params.inner.redaction_params =
      RedactionParams({url::Origin::Create(GURL(kOtherOriginUrl))}, {});

  auto main_frame_response = NewMockPaintPreviewCaptureResponse();
  main_frame_response->embedding_token = std::nullopt;
  main_frame_response->geometry_metadata->scroll_offsets = gfx::Point(0, 0);
  main_frame_response->geometry_metadata->frame_offsets = gfx::Point(0, 0);

  auto subframe_response = mojom::GeometryMetadataResponse::New();
  subframe_response->scroll_offsets = gfx::Point(0, 0);
  subframe_response->frame_offsets = gfx::Point(0, 0);

  MockPaintPreviewRecorder main_frame_recorder;
  main_frame_recorder.SetExpectedParams(ToMojoParams(main_frame_params));
  main_frame_recorder.SetResponse(std::move(main_frame_response));
  base::test::TestFuture<void> main_frame_req;
  main_frame_recorder.SetReceivedRequestClosure(main_frame_req.GetCallback());
  OverrideInterface(rfh, &main_frame_recorder);

  const gfx::Rect subframe_rect(0, 0, 100, 200);

  MockPaintPreviewRecorder subframe_recorder;
  auto subframe_geo_params = mojom::GeometryMetadataParams::New();
  subframe_geo_params->clip_rect = subframe_rect;
  subframe_recorder.SetExpectedGeometryParams(subframe_geo_params.Clone());
  subframe_recorder.SetGeometryResponse(subframe_response.Clone());
  base::test::TestFuture<void> subframe_req;
  subframe_recorder.SetReceivedRequestClosure(subframe_req.GetCallback());
  OverrideInterface(subframe, &subframe_recorder);

  PaintPreviewClient::CreateForWebContents(web_contents());
  auto* client = PaintPreviewClient::FromWebContents(web_contents());
  ASSERT_NE(client, nullptr);

  CaptureResultFuture main_capture_future;
  client->CapturePaintPreview(main_frame_params.Clone(), rfh,
                              main_capture_future.GetCallback());
  ASSERT_TRUE(main_frame_req.Wait());

  client->CaptureSubframePaintPreview(
      main_frame_params.inner.get_document_guid(), subframe_rect, subframe);
  ASSERT_TRUE(subframe_req.Wait());

  switch (ordering()) {
    case ResponseOrdering::kMainFrameThenSubframe:
      main_frame_recorder.SendResponse();
      subframe_recorder.SendGeometryResponse();
      break;
    case ResponseOrdering::kSubframeThenMainFrame:
      subframe_recorder.SendGeometryResponse();
      main_frame_recorder.SendResponse();
      break;
  }

  auto [main_guid, main_status, result] = main_capture_future.Take();
  EXPECT_EQ(main_guid, main_frame_params.inner.get_document_guid());
  EXPECT_EQ(main_status, mojom::PaintPreviewStatus::kOk);

  sk_sp<SkPicture> skp;
  switch (persistence()) {
    case RecordingPersistence::kFileSystem:
      VerifyFilePath(result->proto.root_frame().file_path());
      ASSERT_EQ(result->proto.subframes().size(), 1);
      VerifyFilePath(result->proto.subframes(0).file_path());
      skp = ReadPictureFromFile(result->proto.subframes(0).file_path());
      break;

    case RecordingPersistence::kMemoryBuffer: {
      auto subframe_token = DeserializeFrameToken(result->proto.subframes(0));
      ASSERT_THAT(result->serialized_skps,
                  UnorderedElementsAre(
                      Key(DeserializeFrameToken(result->proto.root_frame())),
                      Key(subframe_token)));
      skp = ReadPictureFromBuffer(
          std::move(result->serialized_skps.at(subframe_token)));
      break;
    }
  }

  SkBitmap subframe_bitmap = MakeBitmapFromPicture(skp);

  EXPECT_EQ(subframe_bitmap.width(), subframe_rect.width());
  EXPECT_EQ(subframe_bitmap.height(), subframe_rect.height());

  EXPECT_EQ(subframe_bitmap.getColor(0, 0), SK_ColorBLACK);
  EXPECT_EQ(subframe_bitmap.getColor(subframe_bitmap.width() - 1,
                                     subframe_bitmap.height() - 1),
            SK_ColorBLACK);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PaintPreviewClientRenderViewHostResponseOrderingTest,
    testing::Combine(testing::Values(RecordingPersistence::kFileSystem,
                                     RecordingPersistence::kMemoryBuffer),
                     testing::Values(ResponseOrdering::kMainFrameThenSubframe,
                                     ResponseOrdering::kSubframeThenMainFrame)),
    [](const testing::TestParamInfo<
        std::tuple<paint_preview::RecordingPersistence, ResponseOrdering>>&
           param) {
      return base::StrCat({
          PersistenceToString(std::get<0>(param.param)),
          "_",
          OrderingToString(std::get<1>(param.param)),
      });
    });

}  // namespace paint_preview
