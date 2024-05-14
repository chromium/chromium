// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/player_compositor_delegate.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "components/memory_pressure/fake_memory_pressure_monitor.h"
#include "components/paint_preview/browser/directory_key.h"
#include "components/paint_preview/browser/file_manager.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

namespace {

constexpr std::array<size_t,
                     PlayerCompositorDelegate::PressureLevelCount::kLevels>
    kMaxParallelRequests = {1, 1, 1};
constexpr float kDefaultScaleFactor = 0.75;

class FakePaintPreviewCompositorClient : public PaintPreviewCompositorClient {
 public:
  explicit FakePaintPreviewCompositorClient(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : response_status_(
            mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess),
        token_(base::UnguessableToken::Create()),
        task_runner_(task_runner) {}
  ~FakePaintPreviewCompositorClient() override = default;

  FakePaintPreviewCompositorClient(const FakePaintPreviewCompositorClient&) =
      delete;
  FakePaintPreviewCompositorClient& operator=(
      const FakePaintPreviewCompositorClient&) = delete;

  const std::optional<base::UnguessableToken>& Token() const override {
    return token_;
  }

  void SetDisconnectHandler(base::OnceClosure closure) override {
    disconnect_handler_ = std::move(closure);
  }

  void BeginSeparatedFrameComposite(
      mojom::PaintPreviewBeginCompositeRequestPtr request,
      mojom::PaintPreviewCompositor::BeginSeparatedFrameCompositeCallback
          callback) override {
    auto response = mojom::PaintPreviewBeginCompositeResponse::New();
    response->root_frame_guid = base::UnguessableToken::Create();
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), response_status_,
                                          std::move(response)));
  }

  void BitmapForSeparatedFrame(
      const base::UnguessableToken& frame_guid,
      const gfx::Rect& clip_rect,
      float scale_factor,
      mojom::PaintPreviewCompositor::BitmapForSeparatedFrameCallback callback,
      bool run_task_on_default_task_runner = true) override {
    SkBitmap bitmap;
    bitmap.allocPixels(
        SkImageInfo::MakeN32Premul(clip_rect.width(), clip_rect.height()));
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                       bitmap),
        base::Seconds(1));
  }

  void BeginMainFrameComposite(
      mojom::PaintPreviewBeginCompositeRequestPtr request,
      mojom::PaintPreviewCompositor::BeginMainFrameCompositeCallback callback)
      override {
    auto response = mojom::PaintPreviewBeginCompositeResponse::New();
    response->root_frame_guid = base::UnguessableToken::Create();
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), response_status_,
                                          std::move(response)));
  }

  void BitmapForMainFrame(
      const gfx::Rect& clip_rect,
      float scale_factor,
      mojom::PaintPreviewCompositor::BitmapForMainFrameCallback callback,
      bool run_task_on_default_task_runner = true) override {
    SkBitmap bitmap;
    bitmap.allocPixels(
        SkImageInfo::MakeN32Premul(clip_rect.width(), clip_rect.height()));
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                       bitmap),
        base::Seconds(1));
  }

  void SetRootFrameUrl(const GURL& url) override {
    // no-op
  }

  void SetBeginSeparatedFrameResponseStatus(
      mojom::PaintPreviewCompositor::BeginCompositeStatus status) {
    response_status_ = status;
  }

  void Disconnect() {
    if (disconnect_handler_)
      std::move(disconnect_handler_).Run();
  }

 private:
  mojom::PaintPreviewCompositor::BeginCompositeStatus response_status_;
  std::optional<base::UnguessableToken> token_;
  base::OnceClosure disconnect_handler_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class FakePaintPreviewCompositorService : public PaintPreviewCompositorService {
 public:
  explicit FakePaintPreviewCompositorService(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner), timeout_(false) {}
  ~FakePaintPreviewCompositorService() override = default;

  FakePaintPreviewCompositorService(const FakePaintPreviewCompositorService&) =
      delete;
  FakePaintPreviewCompositorService& operator=(
      const FakePaintPreviewCompositorService&) = delete;

  std::unique_ptr<PaintPreviewCompositorClient, base::OnTaskRunnerDeleter>
  CreateCompositor(base::OnceClosure connected_closure) override {
    task_runner_->PostTask(
        FROM_HERE, timeout_ ? base::DoNothing() : std::move(connected_closure));
    return std::unique_ptr<FakePaintPreviewCompositorClient,
                           base::OnTaskRunnerDeleter>(
        new FakePaintPreviewCompositorClient(task_runner_),
        base::OnTaskRunnerDeleter(task_runner_));
  }

  void OnMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel
                            memory_pressure_level) override {
    // no-op.
  }

  void SetTimeout() { timeout_ = true; }

  bool HasActiveClients() const override {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  void SetDisconnectHandler(base::OnceClosure disconnect_handler) override {
    disconnect_handler_ = std::move(disconnect_handler);
  }

  void Disconnect() {
    if (disconnect_handler_)
      std::move(disconnect_handler_).Run();
  }

 private:
  base::OnceClosure disconnect_handler_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool timeout_;
};

FakePaintPreviewCompositorClient* AsFakeClient(
    PaintPreviewCompositorClient* client) {
  return static_cast<FakePaintPreviewCompositorClient*>(client);
}

FakePaintPreviewCompositorService* AsFakeService(
    PaintPreviewCompositorService* service) {
  return static_cast<FakePaintPreviewCompositorService*>(service);
}

class PlayerCompositorDelegateImpl : public PlayerCompositorDelegate {
 public:
  PlayerCompositorDelegateImpl() = default;
  ~PlayerCompositorDelegateImpl() override = default;

  PlayerCompositorDelegateImpl(const PlayerCompositorDelegateImpl&) = delete;
  PlayerCompositorDelegateImpl& operator=(const PlayerCompositorDelegateImpl&) =
      delete;

  void SetExpected(CompositorStatus status, float scale_factor) {
    expected_status_ = status;
    expected_scale_factor_ = scale_factor;
    status_checked_ = false;
  }

  void SetFakeMemoryPressureMonitor(base::MemoryPressureMonitor* monitor) {
    memory_pressure_monitor_ = monitor;
  }

  bool WasStatusChecked() const { return status_checked_; }

  void OnCompositorReady(
      CompositorStatus compositor_status,
      mojom::PaintPreviewBeginCompositeResponsePtr composite_response,
      float scale_factor,
      std::unique_ptr<ui::AXTreeUpdate> update) override {
    EXPECT_FLOAT_EQ(expected_scale_factor_, scale_factor);
    // Cast to int for easier debugging.
    EXPECT_EQ(static_cast<int>(expected_status_),
              static_cast<int>(compositor_status));
    status_checked_ = true;
  }

 protected:
  base::MemoryPressureMonitor* memory_pressure_monitor() override {
    if (memory_pressure_monitor_)
      return memory_pressure_monitor_;

    return PlayerCompositorDelegate::memory_pressure_monitor();
  }

 private:
  raw_ptr<base::MemoryPressureMonitor> memory_pressure_monitor_{nullptr};
  CompositorStatus expected_status_{CompositorStatus::OK};
  bool status_checked_{false};
  float expected_scale_factor_{0.0};
};

}  // namespace

class PlayerCompositorDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    service_ = std::make_unique<PaintPreviewBaseService>(
        std::make_unique<PaintPreviewFileMixin>(temp_dir.GetPath(), "test"),
        nullptr, false);
  }

  PaintPreviewBaseService* GetBaseService() { return service_.get(); }

  std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
  CreateCompositorService() {
    return std::unique_ptr<FakePaintPreviewCompositorService,
                           base::OnTaskRunnerDeleter>(
        new FakePaintPreviewCompositorService(env.GetMainThreadTaskRunner()),
        base::OnTaskRunnerDeleter(env.GetMainThreadTaskRunner()));
  }

  PaintPreviewProto CreateValidProto(const GURL& url) {
    PaintPreviewProto proto;
    auto* metadata = proto.mutable_metadata();
    metadata->set_url(url.spec());
    metadata->set_version(kPaintPreviewVersion);
    metadata->set_page_scale_factor(kDefaultScaleFactor);

    auto root_frame_id = base::UnguessableToken::Create();
    auto* root_frame = proto.mutable_root_frame();
    root_frame->set_embedding_token_high(
        root_frame_id.GetHighForSerialization());
    root_frame->set_embedding_token_low(root_frame_id.GetLowForSerialization());
    root_frame->set_is_main_frame(true);

    return proto;
  }

  void SerializeProtoAndCreateRootSkp(PaintPreviewProto* proto,
                                      const DirectoryKey& key,
                                      bool skip_proto_serialization = false) {
    auto file_manager = GetBaseService()->GetFileMixin()->GetFileManager();
    base::RunLoop loop;
    file_manager->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::OnceClosure quit, scoped_refptr<FileManager> file_manager,
               PaintPreviewProto* proto, const DirectoryKey& key,
               bool skip_proto_serialization) {
              auto directory = file_manager->CreateOrGetDirectory(key, true);

              std::string fake_data = "Hello World!";
              auto root_file = directory->AppendASCII("0.skp");
              proto->mutable_root_frame()->set_file_path(
                  root_file.AsUTF8Unsafe());
              base::WriteFile(root_file, fake_data);

              if (!skip_proto_serialization) {
                file_manager->SerializePaintPreviewProto(key, *proto, false);
              }
              std::move(quit).Run();
            },
            loop.QuitClosure(), file_manager, proto, key,
            skip_proto_serialization));
    loop.Run();
  }

  base::test::TaskEnvironment env{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<PaintPreviewBaseService> service_;
  base::ScopedTempDir temp_dir;
};

TEST_F(PlayerCompositorDelegateTest, OnClick) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);

  GURL url("www.example.com");
  PaintPreviewProto proto = CreateValidProto(url);

  GURL root_frame_link("www.chromium.org");
  auto root_frame_id = base::UnguessableToken::Create();

  auto* root_frame = proto.mutable_root_frame();
  root_frame->set_embedding_token_high(root_frame_id.GetHighForSerialization());
  root_frame->set_embedding_token_low(root_frame_id.GetLowForSerialization());
  auto* root_frame_link_proto = root_frame->add_links();
  root_frame_link_proto->set_url(root_frame_link.spec());
  auto* root_frame_rect_proto = root_frame_link_proto->mutable_rect();
  root_frame_rect_proto->set_x(10);
  root_frame_rect_proto->set_y(20);
  root_frame_rect_proto->set_width(30);
  root_frame_rect_proto->set_height(40);

  GURL subframe_link("www.foo.com");
  auto subframe_id = base::UnguessableToken::Create();

  auto* subframe = proto.add_subframes();
  subframe->set_embedding_token_high(subframe_id.GetHighForSerialization());
  subframe->set_embedding_token_low(subframe_id.GetLowForSerialization());
  subframe->set_is_main_frame(true);
  auto* subframe_link_proto = subframe->add_links();
  subframe_link_proto->set_url(subframe_link.spec());
  auto* subframe_rect_proto = subframe_link_proto->mutable_rect();
  subframe_rect_proto->set_x(1);
  subframe_rect_proto->set_y(2);
  subframe_rect_proto->set_width(3);
  subframe_rect_proto->set_height(4);

  base::RunLoop loop;
  file_manager->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure quit, scoped_refptr<FileManager> file_manager,
             PaintPreviewProto* proto, const DirectoryKey& key) {
            auto directory = file_manager->CreateOrGetDirectory(key, true);

            std::string fake_data = "Hello World!";
            auto root_file = directory->AppendASCII("0.skp");
            proto->mutable_root_frame()->set_file_path(
                root_file.AsUTF8Unsafe());
            base::WriteFile(root_file, fake_data);

            auto subframe_file = directory->AppendASCII("1.skp");
            proto->mutable_subframes(0)->set_file_path(
                subframe_file.AsUTF8Unsafe());
            base::WriteFile(subframe_file, fake_data);

            file_manager->SerializePaintPreviewProto(key, *proto, false);
            std::move(quit).Run();
          },
          loop.QuitClosure(), file_manager, &proto, key));
  loop.Run();

  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::OK,
                                           kDefaultScaleFactor);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, url, key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());

    auto res = player_compositor_delegate.OnClick(root_frame_id,
                                                  gfx::Rect(10, 20, 1, 1));
    ASSERT_EQ(res.size(), 1U);
    EXPECT_EQ(*(res[0]), root_frame_link);

    res = player_compositor_delegate.OnClick(root_frame_id,
                                             gfx::Rect(0, 0, 1, 1));
    EXPECT_TRUE(res.empty());

    res =
        player_compositor_delegate.OnClick(subframe_id, gfx::Rect(1, 2, 1, 1));
    ASSERT_EQ(res.size(), 1U);
    EXPECT_EQ(*(res[0]), subframe_link);
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, BadProto) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  base::RunLoop loop;
  file_manager->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure quit, scoped_refptr<FileManager> file_manager,
             const DirectoryKey& key) {
            auto directory = file_manager->CreateOrGetDirectory(key, true);
            std::string fake_data = "Hello World!";
            auto proto_file = directory->AppendASCII("proto.pb");
            base::WriteFile(proto_file, fake_data);
          },
          loop.QuitClosure(), file_manager, key));

  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(
        CompositorStatus::PROTOBUF_DESERIALIZATION_ERROR, 0.0);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, GURL(), key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, OldVersion) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  GURL url("https://www.chromium.org/");
  auto proto = CreateValidProto(url);
  proto.mutable_metadata()->set_version(kPaintPreviewVersion - 1);
  SerializeProtoAndCreateRootSkp(&proto, key);
  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::OLD_VERSION, 0.0);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, url, key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    player_compositor_delegate.SetCompressOnClose(false);
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, InMemoryProto) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  GURL url("https://www.chromium.org/");
  auto proto = CreateValidProto(url);
  SerializeProtoAndCreateRootSkp(&proto, key, true);
  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    auto capture_result =
        std::make_unique<CaptureResult>(RecordingPersistence::kFileSystem);
    capture_result->proto = std::move(proto);
    player_compositor_delegate.SetCaptureResult(std::move(capture_result));
    player_compositor_delegate.SetExpected(CompositorStatus::OK,
                                           kDefaultScaleFactor);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, url, key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    player_compositor_delegate.SetCompressOnClose(false);
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, URLMismatch) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  GURL url("https://www.chromium.org/");
  auto proto = CreateValidProto(url);
  SerializeProtoAndCreateRootSkp(&proto, key);
  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::URL_MISMATCH, 0.0);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, GURL(), key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, ServiceDisconnect) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  GURL url("https://www.chromium.org/");
  auto proto = CreateValidProto(url);
  SerializeProtoAndCreateRootSkp(&proto, key);
  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::OK,
                                           kDefaultScaleFactor);
    bool called = false;
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, url, key, /*main_frame_mode=*/false,
        base::BindOnce(
            [](bool* called, int status) {
              EXPECT_EQ(static_cast<int>(
                            CompositorStatus::COMPOSITOR_SERVICE_DISCONNECT),
                        status);
              *called = true;
            },
            &called),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    AsFakeService(player_compositor_delegate.GetCompositorServiceForTest())
        ->Disconnect();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());
    EXPECT_TRUE(called);
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, ClientDisconnect) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  GURL url("https://www.chromium.org/");
  auto proto = CreateValidProto(url);
  SerializeProtoAndCreateRootSkp(&proto, key);
  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::OK,
                                           kDefaultScaleFactor);
    bool called = false;
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, url, key, /*main_frame_mode=*/false,
        base::BindOnce(
            [](bool* called, int status) {
              EXPECT_EQ(static_cast<int>(
                            CompositorStatus::COMPOSITOR_CLIENT_DISCONNECT),
                        status);
              *called = true;
            },
            &called),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    AsFakeClient(player_compositor_delegate.GetClientForTest())->Disconnect();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());
    EXPECT_TRUE(called);
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, InvalidCompositeRequest) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  GURL url("https://www.chromium.org/");
  auto proto = CreateValidProto(url);
  base::RunLoop loop;
  file_manager->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure quit, scoped_refptr<FileManager> file_manager,
             PaintPreviewProto* proto, const DirectoryKey& key) {
            file_manager->CreateOrGetDirectory(key, true);
            file_manager->SerializePaintPreviewProto(key, *proto, false);
            std::move(quit).Run();
          },
          loop.QuitClosure(), file_manager, &proto, key));
  loop.Run();
  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::INVALID_REQUEST,
                                           0.0);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, url, key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, CompositorDeserializationError) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  GURL url("https://www.chromium.org/");
  auto proto = CreateValidProto(url);
  SerializeProtoAndCreateRootSkp(&proto, key);
  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(
        CompositorStatus::COMPOSITOR_DESERIALIZATION_ERROR, 0.0);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, url, key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    AsFakeClient(player_compositor_delegate.GetClientForTest())
        ->SetBeginSeparatedFrameResponseStatus(
            mojom::PaintPreviewCompositor::BeginCompositeStatus::
                kDeserializingFailure);
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, InvalidRootSkp) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  GURL url("https://www.chromium.org/");
  auto proto = CreateValidProto(url);
  SerializeProtoAndCreateRootSkp(&proto, key);
  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(
        CompositorStatus::INVALID_ROOT_FRAME_SKP, 0.0);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, url, key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    AsFakeClient(player_compositor_delegate.GetClientForTest())
        ->SetBeginSeparatedFrameResponseStatus(
            mojom::PaintPreviewCompositor::BeginCompositeStatus::
                kCompositingFailure);
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, CompressOnClose) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  base::FilePath dir;
  file_manager->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, file_manager, key,
                     false),
      base::BindOnce(
          [](base::FilePath* out,
             const std::optional<base::FilePath>& file_path) {
            *out = file_path.value();
          },
          base::Unretained(&dir)));
  env.RunUntilIdle();
  std::string data = "foo";
  EXPECT_TRUE(base::WriteFile(dir.AppendASCII("test_file"), data));
  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::NO_CAPTURE, 0.0);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, GURL(), key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());
  }
  env.RunUntilIdle();
  EXPECT_TRUE(base::PathExists(dir.AddExtensionASCII(".zip")));
}

TEST_F(PlayerCompositorDelegateTest, RequestBitmapWithCancel) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  {
    // This test skips setting up files as the fakes don't use them. In normal
    // execution the files are required by the service or no bitmap will be
    // created.
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::NO_CAPTURE, 0.0);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, GURL(), key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());

    base::RunLoop loop0;
    int request_0 = player_compositor_delegate.RequestBitmap(
        base::UnguessableToken::Create(), gfx::Rect(10, 20, 30, 40), 1.0,
        base::BindOnce(
            [](base::OnceClosure quit,
               mojom::PaintPreviewCompositor::BitmapStatus status,
               const SkBitmap& bitmap) {
              EXPECT_EQ(mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                        status);
              std::move(quit).Run();
            },
            loop0.QuitClosure()));
    bool request_1_called = false;
    int request_1 = player_compositor_delegate.RequestBitmap(
        base::UnguessableToken::Create(), gfx::Rect(10, 20, 30, 40), 1.0,
        base::BindOnce(
            [](bool* called, mojom::PaintPreviewCompositor::BitmapStatus status,
               const SkBitmap& bitmap) { *called = true; },
            &request_1_called));
    base::RunLoop loop2;
    int request_2 = player_compositor_delegate.RequestBitmap(
        base::UnguessableToken::Create(), gfx::Rect(10, 20, 30, 40), 1.0,
        base::BindOnce(
            [](base::OnceClosure quit,
               mojom::PaintPreviewCompositor::BitmapStatus status,
               const SkBitmap& bitmap) {
              EXPECT_EQ(mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                        status);
              std::move(quit).Run();
            },
            loop2.QuitClosure()));
    bool request_3_called = false;
    int request_3 = player_compositor_delegate.RequestBitmap(
        base::UnguessableToken::Create(), gfx::Rect(10, 20, 30, 40), 1.0,
        base::BindOnce(
            [](bool* called, mojom::PaintPreviewCompositor::BitmapStatus status,
               const SkBitmap& bitmap) { *called = true; },
            &request_3_called));
    EXPECT_EQ(
        std::set<int>({request_0, request_1, request_2, request_3}).size(), 4U);

    EXPECT_FALSE(player_compositor_delegate.CancelBitmapRequest(request_0));
    EXPECT_TRUE(player_compositor_delegate.CancelBitmapRequest(request_1));
    EXPECT_FALSE(player_compositor_delegate.CancelBitmapRequest(request_1));
    EXPECT_TRUE(player_compositor_delegate.CancelBitmapRequest(request_3));

    loop0.Run();
    loop2.Run();
    env.RunUntilIdle();
    EXPECT_FALSE(request_1_called);
    EXPECT_FALSE(request_3_called);
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, RequestBitmapWithCancelAll) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  {
    // This test skips setting up files as the fakes don't use them. In normal
    // execution the files are required by the service or no bitmap will be
    // created.
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::NO_CAPTURE, 0.0);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, GURL(), key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());

    base::RunLoop loop0;
    int request_0 = player_compositor_delegate.RequestBitmap(
        base::UnguessableToken::Create(), gfx::Rect(10, 20, 30, 40), 1.0,
        base::BindOnce(
            [](base::OnceClosure quit,
               mojom::PaintPreviewCompositor::BitmapStatus status,
               const SkBitmap& bitmap) {
              EXPECT_EQ(mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                        status);
              std::move(quit).Run();
            },
            loop0.QuitClosure()));
    bool request_1_called = false;
    int request_1 = player_compositor_delegate.RequestBitmap(
        base::UnguessableToken::Create(), gfx::Rect(10, 20, 30, 40), 1.0,
        base::BindOnce(
            [](bool* called, mojom::PaintPreviewCompositor::BitmapStatus status,
               const SkBitmap& bitmap) { *called = true; },
            &request_1_called));
    bool request_2_called = false;
    int request_2 = player_compositor_delegate.RequestBitmap(
        base::UnguessableToken::Create(), gfx::Rect(10, 20, 30, 40), 1.0,
        base::BindOnce(
            [](bool* called, mojom::PaintPreviewCompositor::BitmapStatus status,
               const SkBitmap& bitmap) { *called = true; },
            &request_2_called));
    EXPECT_EQ(std::set<int>({request_0, request_1, request_2}).size(), 3U);
    player_compositor_delegate.CancelAllBitmapRequests();
    loop0.Run();
    env.RunUntilIdle();
    EXPECT_FALSE(request_1_called);
    EXPECT_FALSE(request_2_called);
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, RequestBitmapSuccessQueued) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  {
    // This test skips setting up files as the fakes don't use them. In normal
    // execution the files are required by the service or no bitmap will be
    // created.
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::NO_CAPTURE, 0.0);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, GURL(), key, /*main_frame_mode=*/false, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());

    base::RunLoop loop_1;
    base::RunLoop loop_2;
    player_compositor_delegate.RequestBitmap(
        base::UnguessableToken::Create(), gfx::Rect(10, 20, 30, 40), 1.0,
        base::BindOnce(
            [](base::OnceClosure quit,
               mojom::PaintPreviewCompositor::BitmapStatus status,
               const SkBitmap& bitmap) {
              EXPECT_EQ(mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                        status);
              std::move(quit).Run();
            },
            loop_1.QuitClosure()));
    player_compositor_delegate.RequestBitmap(
        base::UnguessableToken::Create(), gfx::Rect(10, 20, 30, 40), 1.0,
        base::BindOnce(
            [](base::OnceClosure quit,
               mojom::PaintPreviewCompositor::BitmapStatus status,
               const SkBitmap& bitmap) {
              EXPECT_EQ(mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                        status);
              std::move(quit).Run();
            },
            loop_2.QuitClosure()));
    loop_1.Run();
    loop_2.Run();
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, RequestMainFrameBitmapSuccess) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  GURL url("https://www.chromium.org/");
  auto proto = CreateValidProto(url);
  SerializeProtoAndCreateRootSkp(&proto, key);
  {
    // This test skips setting up files as the fakes don't use them. In normal
    // execution the files are required by the service or no bitmap will be
    // created.
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::OK,
                                           kDefaultScaleFactor);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, url, key, /*main_frame_mode=*/true, base::DoNothing(),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());

    base::RunLoop loop;
    player_compositor_delegate.RequestBitmap(
        std::nullopt, gfx::Rect(10, 20, 30, 40), 1.0,
        base::BindOnce(
            [](base::OnceClosure quit,
               mojom::PaintPreviewCompositor::BitmapStatus status,
               const SkBitmap& bitmap) {
              EXPECT_EQ(mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                        status);
              std::move(quit).Run();
            },
            loop.QuitClosure()));
    loop.Run();
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, Timeout) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  {
    PlayerCompositorDelegateImpl player_compositor_delegate;
    auto compositor_service = CreateCompositorService();
    AsFakeService(compositor_service.get())->SetTimeout();
    base::RunLoop loop;
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, GURL(), key, /*main_frame_mode=*/false,
        base::BindOnce(
            [](base::OnceClosure quit, int status) {
              EXPECT_EQ(static_cast<CompositorStatus>(status),
                        CompositorStatus::TIMED_OUT);
              std::move(quit).Run();
            },
            loop.QuitClosure()),
        base::Seconds(1), kMaxParallelRequests, std::move(compositor_service));
    env.FastForwardBy(base::Seconds(5));
    loop.Run();
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, CriticalMemoryPressure) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  {
    // This test skips setting up files as the fakes don't use them. In normal
    // execution the files are required by the service or no bitmap will be
    // created.
    base::RunLoop loop;
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::NO_CAPTURE, 0.0);
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, GURL(), key, /*main_frame_mode=*/false,
        base::BindOnce(
            [](base::OnceClosure quit, int compositor_status) {
              EXPECT_EQ(compositor_status,
                        static_cast<int>(
                            CompositorStatus::STOPPED_DUE_TO_MEMORY_PRESSURE));
              std::move(quit).Run();
            },
            loop.QuitClosure()),
        base::TimeDelta::Max(), kMaxParallelRequests,
        CreateCompositorService());
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());

    player_compositor_delegate.OnMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
    loop.Run();
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest, CriticalMemoryPressureBeforeStart) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  {
    // This test skips setting up files as the fakes don't use them. In normal
    // execution the files are required by the service or no bitmap will be
    // created.
    base::RunLoop loop;
    memory_pressure::test::FakeMemoryPressureMonitor memory_pressure_monitor;
    memory_pressure_monitor.SetAndNotifyMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetFakeMemoryPressureMonitor(
        &memory_pressure_monitor);
    player_compositor_delegate.Initialize(
        service, GURL(), key, /*main_frame_mode=*/false,
        base::BindOnce(
            [](base::OnceClosure quit, int compositor_status) {
              EXPECT_EQ(compositor_status,
                        static_cast<int>(
                            CompositorStatus::SKIPPED_DUE_TO_MEMORY_PRESSURE));
              std::move(quit).Run();
            },
            loop.QuitClosure()),
        base::TimeDelta::Max(), kMaxParallelRequests);
    env.RunUntilIdle();

    player_compositor_delegate.OnMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
    loop.Run();
  }
  env.RunUntilIdle();
}

TEST_F(PlayerCompositorDelegateTest,
       RequestBitmapSuccessQueuedWithPressureAbort) {
  auto* service = GetBaseService();
  auto file_manager = service->GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(1U);
  GURL url("https://www.chromium.org/");
  auto proto = CreateValidProto(url);
  SerializeProtoAndCreateRootSkp(&proto, key);
  {
    // This test skips setting up files as the fakes don't use them. In normal
    // execution the files are required by the service or no bitmap will be
    // created.
    PlayerCompositorDelegateImpl player_compositor_delegate;
    player_compositor_delegate.SetExpected(CompositorStatus::OK,
                                           kDefaultScaleFactor);
    base::RunLoop loop;
    player_compositor_delegate.InitializeWithFakeServiceForTest(
        service, url, key, /*main_frame_mode=*/false,
        base::BindOnce(
            [](base::OnceClosure quit, int compositor_status) {
              EXPECT_EQ(compositor_status,
                        static_cast<int>(
                            CompositorStatus::STOPPED_DUE_TO_MEMORY_PRESSURE));
              std::move(quit).Run();
            },
            loop.QuitClosure()),
        base::TimeDelta::Max(), {1, 0, 0}, CreateCompositorService());
    env.RunUntilIdle();
    EXPECT_TRUE(player_compositor_delegate.WasStatusChecked());

    base::RunLoop request_loop;
    player_compositor_delegate.RequestBitmap(
        base::UnguessableToken::Create(), gfx::Rect(10, 20, 30, 40), 1.0,
        base::BindOnce(
            [](base::OnceClosure quit,
               mojom::PaintPreviewCompositor::BitmapStatus status,
               const SkBitmap& bitmap) {
              EXPECT_EQ(mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                        status);
              std::move(quit).Run();
            },
            request_loop.QuitClosure()));
    request_loop.Run();

    player_compositor_delegate.OnMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
    loop.Run();
  }
  env.RunUntilIdle();
}

}  // namespace paint_preview
