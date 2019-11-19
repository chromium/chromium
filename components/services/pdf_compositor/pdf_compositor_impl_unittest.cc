// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/crash/core/common/crash_key.h"
#include "components/services/pdf_compositor/pdf_compositor_impl.h"
#include "components/services/pdf_compositor/public/cpp/pdf_service_mojo_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

struct TestRequestData {
  uint64_t frame_guid;
  int page_num;
};

class MockPdfCompositorImpl : public PdfCompositorImpl {
 public:
  MockPdfCompositorImpl()
      : PdfCompositorImpl(mojo::NullReceiver(),
                          false /* initialize_environment */,
                          nullptr /* io_task_runner */) {}
  ~MockPdfCompositorImpl() override = default;

  MOCK_METHOD2(OnFulfillRequest, void(uint64_t, int));

 protected:
  void FulfillRequest(base::ReadOnlySharedMemoryMapping serialized_content,
                      const ContentToFrameMap& subframe_content_map,
                      CompositeToPdfCallback callback) override {
    const auto* data = serialized_content.GetMemoryAs<const TestRequestData>();
    OnFulfillRequest(data->frame_guid, data->page_num);
  }
};

// MockCompletionPdfCompositorImpl is used for testing related to
// Prepare/Complete document pipeline.
class MockCompletionPdfCompositorImpl : public PdfCompositorImpl {
 public:
  MockCompletionPdfCompositorImpl()
      : PdfCompositorImpl(mojo::NullReceiver(),
                          false /* initialize_environment */,
                          nullptr /* io_task_runner */) {}
  ~MockCompletionPdfCompositorImpl() override = default;

  MOCK_CONST_METHOD0(OnCompleteDocumentRequest, void());
  MOCK_METHOD2(OnCompositeToPdf, void(uint64_t, int));

 protected:
  mojom::PdfCompositor::Status CompositeToPdf(
      base::ReadOnlySharedMemoryMapping shared_mem,
      const ContentToFrameMap& subframe_content_map,
      base::ReadOnlySharedMemoryRegion* region) override {
    const auto* data = shared_mem.GetMemoryAs<const TestRequestData>();
    if (docinfo_)
      docinfo_->pages_written++;
    OnCompositeToPdf(data->frame_guid, data->page_num);
    return mojom::PdfCompositor::Status::kSuccess;
  }

  void CompleteDocumentRequest(
      CompleteDocumentToPdfCallback callback) override {
    OnCompleteDocumentRequest();
  }
};

class PdfCompositorImplTest : public testing::Test {
 public:
  PdfCompositorImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        run_loop_(std::make_unique<base::RunLoop>()),
        is_ready_(false) {}

  void OnIsReadyToCompositeCallback(bool is_ready) {
    is_ready_ = is_ready;
    run_loop_->Quit();
  }

  bool ResultFromCallback() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
    return is_ready_;
  }

  static void OnCompositeToPdfCallback(
      mojom::PdfCompositor::Status status,
      base::ReadOnlySharedMemoryRegion region) {
    // A stub for testing, no implementation.
  }

  static void OnPrepareForDocumentToPdfCallback(
      mojom::PdfCompositor::Status status) {
    // A stub for testing, no implementation.
  }

  void OnCompositeOrCompleteDocumentToPdfCallback(
      mojom::PdfCompositor::Status status,
      base::ReadOnlySharedMemoryRegion region) {
    // A stub for testing, only care about status.
    status_ = status;
  }

  static base::ReadOnlySharedMemoryRegion CreateTestData(uint64_t frame_guid,
                                                         int page_num) {
    static constexpr size_t kSize = sizeof(TestRequestData);
    auto region = base::ReadOnlySharedMemoryRegion::Create(kSize);
    auto* data = region.mapping.GetMemoryAs<TestRequestData>();
    data->frame_guid = frame_guid;
    data->page_num = page_num;
    return std::move(region.region);
  }

  mojom::PdfCompositor::Status GetStatus() const { return status_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  bool is_ready_;
  mojom::PdfCompositor::Status status_ = mojom::PdfCompositor::Status::kSuccess;
};

class PdfCompositorImplCrashKeyTest : public PdfCompositorImplTest {
 public:
  PdfCompositorImplCrashKeyTest() {}
  ~PdfCompositorImplCrashKeyTest() override {}

  void SetUp() override {
    crash_reporter::ResetCrashKeysForTesting();
    crash_reporter::InitializeCrashKeys();
  }

  void TearDown() override { crash_reporter::ResetCrashKeysForTesting(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(PdfCompositorImplCrashKeyTest);
};

TEST_F(PdfCompositorImplTest, IsReadyToComposite) {
  PdfCompositorImpl impl(mojo::NullReceiver(),
                         false /* initialize_environment */,
                         nullptr /* io_task_runner */);
  // Frame 2 and 3 are painted.
  impl.AddSubframeContent(2, CreateTestData(2, -1), ContentToFrameMap());
  impl.AddSubframeContent(3, CreateTestData(3, -1), ContentToFrameMap());

  // Frame 1 contains content 3 which corresponds to frame 2.
  // Frame 1 should be ready as frame 2 is ready.
  ContentToFrameMap subframe_content_map = {{3, 2}};
  base::flat_set<uint64_t> pending_subframes;
  EXPECT_TRUE(
      impl.IsReadyToComposite(1, subframe_content_map, &pending_subframes));
  EXPECT_TRUE(pending_subframes.empty());

  // If another page of frame 1 needs content 2 which corresponds to frame 3.
  // This page is ready since frame 3 was painted also.
  subframe_content_map = {{2, 3}};
  EXPECT_TRUE(
      impl.IsReadyToComposite(1, subframe_content_map, &pending_subframes));
  EXPECT_TRUE(pending_subframes.empty());

  // Frame 1 with content 1, 2 and 3 should not be ready since content 1's
  // content in frame 4 is not painted yet.
  subframe_content_map = {{1, 4}, {2, 3}, {3, 2}};
  EXPECT_FALSE(
      impl.IsReadyToComposite(1, subframe_content_map, &pending_subframes));
  ASSERT_EQ(pending_subframes.size(), 1u);
  EXPECT_EQ(*pending_subframes.begin(), 4u);

  // Add content of frame 4. Now it is ready for composition.
  impl.AddSubframeContent(4, CreateTestData(4, -1), ContentToFrameMap());
  EXPECT_TRUE(
      impl.IsReadyToComposite(1, subframe_content_map, &pending_subframes));
  EXPECT_TRUE(pending_subframes.empty());
}

TEST_F(PdfCompositorImplTest, MultiLayerDependency) {
  PdfCompositorImpl impl(mojo::NullReceiver(),
                         false /* initialize_environment */,
                         nullptr /* io_task_runner */);
  // Frame 3 has content 1 which refers to subframe 1.
  ContentToFrameMap subframe_content_map = {{1, 1}};
  impl.AddSubframeContent(3, CreateTestData(3, -1), subframe_content_map);

  // Frame 5 has content 3 which refers to subframe 3.
  // Although frame 3's content is added, its subframe 1's content is not added.
  // So frame 5 is not ready.
  subframe_content_map = {{3, 3}};
  base::flat_set<uint64_t> pending_subframes;
  EXPECT_FALSE(
      impl.IsReadyToComposite(5, subframe_content_map, &pending_subframes));
  ASSERT_EQ(pending_subframes.size(), 1u);
  EXPECT_EQ(*pending_subframes.begin(), 1u);

  // Frame 6 is not ready either since it needs frame 5 to be ready.
  subframe_content_map = {{1, 5}};
  EXPECT_FALSE(
      impl.IsReadyToComposite(6, subframe_content_map, &pending_subframes));
  ASSERT_EQ(pending_subframes.size(), 1u);
  EXPECT_EQ(*pending_subframes.begin(), 5u);

  // When frame 1's content is added, frame 5 is ready.
  impl.AddSubframeContent(1, CreateTestData(1, -1), ContentToFrameMap());
  subframe_content_map = {{3, 3}};
  EXPECT_TRUE(
      impl.IsReadyToComposite(5, subframe_content_map, &pending_subframes));
  EXPECT_TRUE(pending_subframes.empty());

  // Add frame 5's content.
  impl.AddSubframeContent(5, CreateTestData(5, -1), subframe_content_map);

  // Frame 6 is ready too.
  subframe_content_map = {{1, 5}};
  EXPECT_TRUE(
      impl.IsReadyToComposite(6, subframe_content_map, &pending_subframes));
  EXPECT_TRUE(pending_subframes.empty());
}

TEST_F(PdfCompositorImplTest, DependencyLoop) {
  PdfCompositorImpl impl(mojo::NullReceiver(),
                         false /* initialize_environment */,
                         nullptr /* io_task_runner */);
  // Frame 3 has content 1, which refers to frame 1.
  // Frame 1 has content 3, which refers to frame 3.
  ContentToFrameMap subframe_content_map = {{3, 3}};
  impl.AddSubframeContent(1, CreateTestData(1, -1), subframe_content_map);

  subframe_content_map = {{1, 1}};
  impl.AddSubframeContent(3, CreateTestData(3, -1), subframe_content_map);

  // Both frame 1 and 3 are painted, frame 5 should be ready.
  base::flat_set<uint64_t> pending_subframes;
  subframe_content_map = {{1, 3}};
  EXPECT_TRUE(
      impl.IsReadyToComposite(5, subframe_content_map, &pending_subframes));
  EXPECT_TRUE(pending_subframes.empty());

  // Frame 6 has content 7, which refers to frame 7.
  subframe_content_map = {{7, 7}};
  impl.AddSubframeContent(6, CreateTestData(6, -1), subframe_content_map);
  // Frame 7 should be ready since frame 6's own content is added and it only
  // depends on frame 7.
  subframe_content_map = {{6u, 6u}};
  EXPECT_TRUE(
      impl.IsReadyToComposite(7, subframe_content_map, &pending_subframes));
  EXPECT_TRUE(pending_subframes.empty());
}

TEST_F(PdfCompositorImplTest, MultiRequestsBasic) {
  MockPdfCompositorImpl impl;
  // Page 0 with frame 3 has content 1, which refers to frame 8.
  // When the content is not available, the request is not fulfilled.
  const ContentToFrameMap subframe_content_map = {{1, 8}};
  EXPECT_CALL(impl, OnFulfillRequest(testing::_, testing::_)).Times(0);
  impl.CompositePageToPdf(
      3, CreateTestData(3, 0), subframe_content_map,
      base::BindOnce(&PdfCompositorImplTest::OnCompositeToPdfCallback));
  testing::Mock::VerifyAndClearExpectations(&impl);

  // When frame 8's content is ready, the previous request should be fulfilled.
  EXPECT_CALL(impl, OnFulfillRequest(3, 0)).Times(1);
  impl.AddSubframeContent(8, CreateTestData(8, -1), ContentToFrameMap());
  testing::Mock::VerifyAndClearExpectations(&impl);

  // The following requests which only depends on frame 8 should be
  // immediately fulfilled.
  EXPECT_CALL(impl, OnFulfillRequest(3, 1)).Times(1);
  EXPECT_CALL(impl, OnFulfillRequest(3, -1)).Times(1);
  impl.CompositePageToPdf(
      3, CreateTestData(3, 1), subframe_content_map,
      base::BindOnce(&PdfCompositorImplTest::OnCompositeToPdfCallback));

  impl.CompositeDocumentToPdf(
      3, CreateTestData(3, -1), subframe_content_map,
      base::BindOnce(&PdfCompositorImplTest::OnCompositeToPdfCallback));
}

TEST_F(PdfCompositorImplTest, MultiRequestsOrder) {
  MockPdfCompositorImpl impl;
  // Page 0 with frame 3 has content 1, which refers to frame 8.
  // When the content is not available, the request is not fulfilled.
  const ContentToFrameMap subframe_content_map = {{1, 8}};
  EXPECT_CALL(impl, OnFulfillRequest(testing::_, testing::_)).Times(0);
  impl.CompositePageToPdf(
      3, CreateTestData(3, 0), subframe_content_map,
      base::BindOnce(&PdfCompositorImplTest::OnCompositeToPdfCallback));

  // The following requests which only depends on frame 8 should be
  // immediately fulfilled.
  impl.CompositePageToPdf(
      3, CreateTestData(3, 1), subframe_content_map,
      base::BindOnce(&PdfCompositorImplTest::OnCompositeToPdfCallback));

  impl.CompositeDocumentToPdf(
      3, CreateTestData(3, -1), subframe_content_map,
      base::BindOnce(&PdfCompositorImplTest::OnCompositeToPdfCallback));
  testing::Mock::VerifyAndClearExpectations(&impl);

  // When frame 8's content is ready, the previous request should be
  // fulfilled.
  EXPECT_CALL(impl, OnFulfillRequest(3, 0)).Times(1);
  EXPECT_CALL(impl, OnFulfillRequest(3, 1)).Times(1);
  EXPECT_CALL(impl, OnFulfillRequest(3, -1)).Times(1);
  impl.AddSubframeContent(8, CreateTestData(8, -1), ContentToFrameMap());
}

TEST_F(PdfCompositorImplTest, MultiRequestsDepOrder) {
  MockPdfCompositorImpl impl;
  // Page 0 with frame 1 has content 1, which refers to frame
  // 2. When the content is not available, the request is not
  // fulfilled.
  EXPECT_CALL(impl, OnFulfillRequest(testing::_, testing::_)).Times(0);
  ContentToFrameMap subframe_content_map = {{1, 2}};
  impl.CompositePageToPdf(
      1, CreateTestData(1, 0), subframe_content_map,
      base::BindOnce(&PdfCompositorImplTest::OnCompositeToPdfCallback));

  // Page 1 with frame 1 has content 1, which refers to frame
  // 3. When the content is not available, the request is not
  // fulfilled either.
  subframe_content_map = {{1, 3}};
  impl.CompositePageToPdf(
      1, CreateTestData(1, 1), subframe_content_map,
      base::BindOnce(&PdfCompositorImplTest::OnCompositeToPdfCallback));
  testing::Mock::VerifyAndClearExpectations(&impl);

  // When frame 3 and 2 become available, the pending requests should be
  // satisfied, thus be fulfilled in order.
  testing::Sequence order;
  EXPECT_CALL(impl, OnFulfillRequest(1, 1)).Times(1).InSequence(order);
  EXPECT_CALL(impl, OnFulfillRequest(1, 0)).Times(1).InSequence(order);
  impl.AddSubframeContent(3, CreateTestData(3, -1), ContentToFrameMap());
  impl.AddSubframeContent(2, CreateTestData(2, -1), ContentToFrameMap());
}

TEST_F(PdfCompositorImplTest, NotifyUnavailableSubframe) {
  MockPdfCompositorImpl impl;
  // Page 0 with frame 3 has content 1, which refers to frame 8.
  // When the content is not available, the request is not fulfilled.
  const ContentToFrameMap subframe_content_map = {{1, 8}};
  EXPECT_CALL(impl, OnFulfillRequest(testing::_, testing::_)).Times(0);
  impl.CompositePageToPdf(
      3, CreateTestData(3, 0), subframe_content_map,
      base::BindOnce(&PdfCompositorImplTest::OnCompositeToPdfCallback));
  testing::Mock::VerifyAndClearExpectations(&impl);

  // Notifies that frame 8's unavailable, the previous request should be
  // fulfilled.
  EXPECT_CALL(impl, OnFulfillRequest(3, 0)).Times(1);
  impl.NotifyUnavailableSubframe(8);
  testing::Mock::VerifyAndClearExpectations(&impl);
}

TEST_F(PdfCompositorImplCrashKeyTest, SetCrashKey) {
  PdfCompositorImpl impl(mojo::NullReceiver(),
                         false /* initialize_environment */,
                         nullptr /* io_task_runner */);
  std::string url_str("https://www.example.com/");
  GURL url(url_str);
  impl.SetWebContentsURL(url);

  EXPECT_EQ(crash_reporter::GetCrashKeyValue("main-frame-url"), url_str);
}

TEST_F(PdfCompositorImplTest, MultiRequestsBasicCompleteDocument) {
  MockCompletionPdfCompositorImpl impl;
  // Page 0 with frame 3 has content 1, which refers to frame 8.
  // When the content is not available, the request is not fulfilled.
  const ContentToFrameMap subframe_content_map = {{1, 8}};
  impl.PrepareForDocumentToPdf(base::BindOnce(
      &PdfCompositorImplTest::OnPrepareForDocumentToPdfCallback));
  EXPECT_CALL(impl, OnCompositeToPdf(testing::_, testing::_)).Times(0);
  impl.CompositePageToPdf(
      3, CreateTestData(3, 0), subframe_content_map,
      base::BindOnce(&PdfCompositorImplTest::OnCompositeToPdfCallback));
  testing::Mock::VerifyAndClearExpectations(&impl);

  // When frame 8's content is ready, the previous request should be fulfilled.
  EXPECT_CALL(impl, OnCompositeToPdf(testing::_, testing::_)).Times(1);
  impl.AddSubframeContent(8, CreateTestData(8, -1), ContentToFrameMap());
  testing::Mock::VerifyAndClearExpectations(&impl);

  // The following requests which only depends on frame 8 should be
  // immediately fulfilled.
  EXPECT_CALL(impl, OnCompositeToPdf(testing::_, testing::_)).Times(1);
  impl.CompositePageToPdf(
      3, CreateTestData(3, 1), subframe_content_map,
      base::BindOnce(&PdfCompositorImplTest::OnCompositeToPdfCallback));
  testing::Mock::VerifyAndClearExpectations(&impl);

  EXPECT_CALL(impl, OnCompleteDocumentRequest()).Times(1);
  impl.CompleteDocumentToPdf(
      2, base::BindOnce(
             &PdfCompositorImplTest::OnCompositeOrCompleteDocumentToPdfCallback,
             base::Unretained(this)));
  EXPECT_EQ(GetStatus(), mojom::PdfCompositor::Status::kSuccess);
}

}  // namespace printing
