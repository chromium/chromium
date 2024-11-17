// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/frame_node_impl.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/task/task_traits.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"
#include "url/origin.h"

namespace performance_manager {

namespace {

using FrameNodeImplTest = GraphTestHarness;

}  // namespace

TEST_F(FrameNodeImplTest, SafeDowncast) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());
  FrameNode* node = frame.get();
  EXPECT_EQ(frame.get(), FrameNodeImpl::FromNode(node));
  NodeBase* base = frame.get();
  EXPECT_EQ(base, NodeBase::FromNode(node));
  EXPECT_EQ(static_cast<Node*>(node), base->ToNode());
}

using FrameNodeImplDeathTest = FrameNodeImplTest;

TEST_F(FrameNodeImplDeathTest, SafeDowncast) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());
  ASSERT_DEATH_IF_SUPPORTED(PageNodeImpl::FromNodeBase(frame.get()), "");
}

TEST_F(FrameNodeImplTest, AddFrameHierarchyBasic) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto parent_node = CreateFrameNodeAutoId(process.get(), page.get());
  auto child2_node =
      CreateFrameNodeAutoId(process.get(), page.get(), parent_node.get());
  auto child3_node =
      CreateFrameNodeAutoId(process.get(), page.get(), parent_node.get());

  EXPECT_EQ(nullptr, parent_node->parent_frame_node());
  EXPECT_EQ(2u, parent_node->child_frame_nodes().size());
  EXPECT_EQ(parent_node.get(), child2_node->parent_frame_node());
  EXPECT_EQ(parent_node.get(), child3_node->parent_frame_node());
}

TEST_F(FrameNodeImplTest, GetFrameNodeById) {
  auto process_a = CreateNode<ProcessNodeImpl>(
      RenderProcessHostProxy::CreateForTesting(RenderProcessHostId(42)));
  auto process_b = CreateNode<ProcessNodeImpl>(
      RenderProcessHostProxy::CreateForTesting(RenderProcessHostId(43)));
  auto page = CreateNode<PageNodeImpl>();
  auto frame_a1 = CreateFrameNodeAutoId(process_a.get(), page.get());
  auto frame_a2 = CreateFrameNodeAutoId(process_a.get(), page.get());
  auto frame_b1 = CreateFrameNodeAutoId(process_b.get(), page.get());

  EXPECT_EQ(graph()->GetFrameNodeById(process_a->GetRenderProcessHostId(),
                                      frame_a1->render_frame_id()),
            frame_a1.get());
  EXPECT_EQ(graph()->GetFrameNodeById(process_a->GetRenderProcessHostId(),
                                      frame_a2->render_frame_id()),
            frame_a2.get());
  EXPECT_EQ(graph()->GetFrameNodeById(process_b->GetRenderProcessHostId(),
                                      frame_b1->render_frame_id()),
            frame_b1.get());
}

TEST_F(FrameNodeImplTest, NavigationCommitted_SameDocument) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  EXPECT_TRUE(frame_node->GetURL().is_empty());
  EXPECT_FALSE(frame_node->GetOrigin().has_value());
  const GURL kUrl("http://www.foo.com/");
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  frame_node->OnNavigationCommitted(
      kUrl, kOrigin, /*same_document=*/true,
      /*is_served_from_back_forward_cache=*/false);
  EXPECT_EQ(kUrl, frame_node->GetURL());
  // Origin argument is ignored for same-document navigation.
  EXPECT_FALSE(frame_node->GetOrigin().has_value());
}

TEST_F(FrameNodeImplTest, NavigationCommitted_DifferentDocument) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  EXPECT_TRUE(frame_node->GetURL().is_empty());
  EXPECT_FALSE(frame_node->GetOrigin().has_value());
  const GURL kUrl("http://www.foo.com/");
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  frame_node->OnNavigationCommitted(
      kUrl, kOrigin, /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  EXPECT_EQ(kUrl, frame_node->GetURL());
  EXPECT_EQ(kOrigin, frame_node->GetOrigin());
}

TEST_F(FrameNodeImplTest, RemoveChildFrame) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto parent_frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  auto child_frame_node =
      CreateFrameNodeAutoId(process.get(), page.get(), parent_frame_node.get());

  // Ensure correct Parent-child relationships have been established.
  EXPECT_EQ(1u, parent_frame_node->child_frame_nodes().size());
  EXPECT_TRUE(!parent_frame_node->parent_frame_node());
  EXPECT_EQ(0u, child_frame_node->child_frame_nodes().size());
  EXPECT_EQ(parent_frame_node.get(), child_frame_node->parent_frame_node());

  child_frame_node.reset();

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(0u, parent_frame_node->child_frame_nodes().size());
  EXPECT_TRUE(!parent_frame_node->parent_frame_node());
}

namespace {

class LenientMockObserver : public FrameNodeImpl::Observer {
 public:
  LenientMockObserver() = default;
  ~LenientMockObserver() override = default;

  MOCK_METHOD(void, OnFrameNodeAdded, (const FrameNode*), (override));
  MOCK_METHOD(void, OnBeforeFrameNodeRemoved, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnCurrentFrameChanged,
              (const FrameNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void, OnNetworkAlmostIdleChanged, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnFrameLifecycleStateChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void, OnURLChanged, (const FrameNode*, const GURL&), (override));
  MOCK_METHOD(void,
              OnOriginChanged,
              (const FrameNode*, const std::optional<url::Origin>&),
              (override));
  MOCK_METHOD(void, OnIsAdFrameChanged, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnFrameIsHoldingWebLockChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnFrameIsHoldingIndexedDBLockChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnPriorityAndReasonChanged,
              (const FrameNode*, const PriorityAndReason& previous_value),
              (override));
  MOCK_METHOD(void, OnFrameUsesWebRTCChanged, (const FrameNode*), (override));
  MOCK_METHOD(void, OnHadUserActivationChanged, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnHadFormInteractionChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void, OnHadUserEditsChanged, (const FrameNode*), (override));
  MOCK_METHOD(void, OnIsAudibleChanged, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnIsCapturingMediaStreamChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnViewportIntersectionChanged,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnFrameVisibilityChanged,
              (const FrameNode*, FrameNode::Visibility),
              (override));
  MOCK_METHOD(void, OnIsImportantChanged, (const FrameNode*), (override));
  MOCK_METHOD(void,
              OnNonPersistentNotificationCreated,
              (const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnFirstContentfulPaint,
              (const FrameNode*, base::TimeDelta),
              (override));

  void SetCreatedFrameNode(const FrameNode* frame_node) {
    created_frame_node_ = frame_node;
  }

  const FrameNode* created_frame_node() { return created_frame_node_; }

 private:
  raw_ptr<const FrameNode, DanglingUntriaged> created_frame_node_ = nullptr;
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

using testing::_;
using testing::Eq;
using testing::Invoke;
using testing::InvokeWithoutArgs;

}  // namespace

TEST_F(FrameNodeImplTest, ObserverWorks) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();

  MockObserver head_obs;
  MockObserver obs;
  MockObserver tail_obs;
  graph()->AddFrameNodeObserver(&head_obs);
  graph()->AddFrameNodeObserver(&obs);
  graph()->AddFrameNodeObserver(&tail_obs);

  // Remove observers at the head and tail of the list inside a callback, and
  // expect that `obs` is still notified correctly.
  EXPECT_CALL(head_obs, OnFrameNodeAdded(_)).WillOnce(InvokeWithoutArgs([&] {
    graph()->RemoveFrameNodeObserver(&head_obs);
    graph()->RemoveFrameNodeObserver(&tail_obs);
  }));
  // `tail_obs` should not be notified as it was removed.
  EXPECT_CALL(tail_obs, OnFrameNodeAdded(_)).Times(0);

  // Create a frame node and expect a matching call to "OnFrameNodeAdded".
  EXPECT_CALL(obs, OnFrameNodeAdded(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetCreatedFrameNode));
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  testing::Mock::VerifyAndClear(&head_obs);
  testing::Mock::VerifyAndClear(&obs);
  testing::Mock::VerifyAndClear(&tail_obs);

  const FrameNode* raw_frame_node = frame_node.get();
  EXPECT_EQ(raw_frame_node, obs.created_frame_node());

  // Invoke "UpdateCurrentFrame" and expect a "OnCurrentFrameChanged" callback.
  EXPECT_CALL(obs, OnCurrentFrameChanged(raw_frame_node, nullptr));
  FrameNodeImpl::UpdateCurrentFrame(/*previous_frame_node=*/frame_node.get(),
                                    /*current_frame_node=*/nullptr, graph());
  testing::Mock::VerifyAndClear(&obs);

  // Invoke "SetNetworkAlmostIdle" and expect an "OnNetworkAlmostIdleChanged"
  // callback.
  EXPECT_CALL(obs, OnNetworkAlmostIdleChanged(raw_frame_node));
  frame_node->SetNetworkAlmostIdle();
  testing::Mock::VerifyAndClear(&obs);

  // Invoke "SetLifecycleState" and expect an "OnFrameLifecycleStateChanged"
  // callback.
  EXPECT_CALL(obs, OnFrameLifecycleStateChanged(raw_frame_node));
  frame_node->SetLifecycleState(mojom::LifecycleState::kFrozen);
  testing::Mock::VerifyAndClear(&obs);

  // Invoke "OnNonPersistentNotificationCreated" and expect an
  // "OnNonPersistentNotificationCreated" callback.
  EXPECT_CALL(obs, OnNonPersistentNotificationCreated(raw_frame_node));
  frame_node->OnNonPersistentNotificationCreated();
  testing::Mock::VerifyAndClear(&obs);

  // Invoke "OnNavigationCommitted" for a different-document navigation and
  // expect "OnURLChanged", "OnOriginChanged" and "OnNetworkAlmostIdleChanged"
  // notifications (note: navigation resets network idle state).
  const GURL kUrl("http://www.foo.com/");
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  EXPECT_CALL(obs, OnURLChanged(raw_frame_node, GURL()))
      .WillOnce([&](const FrameNode*, const GURL& previous_value) {
        // The observer sees the new URL *and* origin.
        EXPECT_EQ(frame_node->GetURL(), kUrl);
        EXPECT_EQ(frame_node->GetOrigin(), kOrigin);
      });
  EXPECT_CALL(obs, OnOriginChanged(
                       raw_frame_node,
                       // Note: Specifying std::optional<url::Origin>() instead
                       // of std::nullopt so Gmock can deduce the type.
                       std::optional<url::Origin>()))
      .WillOnce([&](const FrameNode*,
                    const std::optional<url::Origin>& previous_value) {
        // The observer sees the new URL *and* origin.
        EXPECT_EQ(frame_node->GetURL(), kUrl);
        EXPECT_EQ(frame_node->GetOrigin(), kOrigin);
      });
  EXPECT_CALL(obs, OnNetworkAlmostIdleChanged(raw_frame_node));
  frame_node->OnNavigationCommitted(
      kUrl, kOrigin, /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  testing::Mock::VerifyAndClear(&obs);

  // Invoke "OnNavigationCommitted" for a same-document navigation. Origin isn't
  // affected.
  const GURL kSameDocumentUrl("http://www.foo.com#same-document");
  EXPECT_CALL(obs, OnURLChanged(raw_frame_node, kUrl));
  frame_node->OnNavigationCommitted(
      kSameDocumentUrl, kOrigin,
      /*same_document=*/true, /*is_served_from_back_forward_cache=*/false);
  testing::Mock::VerifyAndClear(&obs);

  // Re-entrant iteration should work.
  EXPECT_CALL(obs, OnFrameVisibilityChanged(raw_frame_node, _))
      .WillOnce(InvokeWithoutArgs([&] {
        frame_node->SetPriorityAndReason(PriorityAndReason(
            base::TaskPriority::USER_BLOCKING, "test priority"));
      }));
  EXPECT_CALL(obs, OnPriorityAndReasonChanged(raw_frame_node, _));
  frame_node->SetVisibility(FrameNode::Visibility::kVisible);
  testing::Mock::VerifyAndClear(&obs);

  // Release the frame node and expect a call to "OnBeforeFrameNodeRemoved".
  EXPECT_CALL(obs, OnBeforeFrameNodeRemoved(raw_frame_node));
  frame_node.reset();
  testing::Mock::VerifyAndClear(&obs);

  graph()->RemoveFrameNodeObserver(&obs);
}

// This is an end-to-end test of the logic that explodes when recursive
// notifications are dispatched during node creation and removal. There are
// other tests of the individual pieces of logic in NodeBase and
// ObservedProperty.
TEST_F(FrameNodeImplDeathTest, SetPropertyDuringNodeCreation) {
  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();

  // Modifying a property during node addition should explode.
  bool set_property = true;
  EXPECT_CALL(obs, OnFrameNodeAdded(_))
      .WillOnce(Invoke([&](const FrameNode* frame_node) {
        if (set_property) {
          auto* impl = FrameNodeImpl::FromNode(frame_node);
          impl->SetIsAdFrame(true);
        }
      }));
  EXPECT_DCHECK_DEATH(auto frame =
                          CreateFrameNodeAutoId(process.get(), page.get()));

  // Now create the node but don't modify a property so that the mock
  // expectation is satisfied.
  set_property = false;
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());

  // Modifying a property during node removal should explode.
  set_property = true;
  EXPECT_CALL(obs, OnBeforeFrameNodeRemoved(_))
      .WillOnce(Invoke([&](const FrameNode* frame_node) {
        if (set_property) {
          auto* impl = FrameNodeImpl::FromNode(frame_node);
          impl->SetIsAdFrame(true);
        }
      }));
  EXPECT_DCHECK_DEATH(frame.reset());

  // Now remove the node but don't modify a property so that the mock
  // expectation is satisfied.
  set_property = false;
  frame.reset();

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, IsAdFrame) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  // Observer will be notified once when IsAdFrame goes from false to true, and
  // again when it goes from true to false.
  EXPECT_CALL(obs, OnIsAdFrameChanged(frame_node.get())).Times(2);

  EXPECT_FALSE(frame_node->IsAdFrame());
  frame_node->SetIsAdFrame(true);
  EXPECT_TRUE(frame_node->IsAdFrame());
  frame_node->SetIsAdFrame(true);
  EXPECT_TRUE(frame_node->IsAdFrame());

  frame_node->SetIsAdFrame(false);
  EXPECT_FALSE(frame_node->IsAdFrame());

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, IsHoldingWebLock) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  EXPECT_FALSE(frame_node->IsHoldingWebLock());
  EXPECT_CALL(obs, OnFrameIsHoldingWebLockChanged(frame_node.get()));
  frame_node->SetIsHoldingWebLock(true);
  EXPECT_TRUE(frame_node->IsHoldingWebLock());
  EXPECT_CALL(obs, OnFrameIsHoldingWebLockChanged(frame_node.get()));
  frame_node->SetIsHoldingWebLock(false);
  EXPECT_FALSE(frame_node->IsHoldingWebLock());

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, IsHoldingIndexedDBLock) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  EXPECT_CALL(obs, OnFrameIsHoldingIndexedDBLockChanged(frame_node.get()));
  frame_node->SetIsHoldingIndexedDBLock(true);
  EXPECT_TRUE(frame_node->IsHoldingIndexedDBLock());
  EXPECT_CALL(obs, OnFrameIsHoldingIndexedDBLockChanged(frame_node.get()));
  frame_node->SetIsHoldingIndexedDBLock(false);
  EXPECT_FALSE(frame_node->IsHoldingIndexedDBLock());

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, UsesWebRTC) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  EXPECT_CALL(obs, OnFrameUsesWebRTCChanged(frame_node.get()));
  frame_node->OnStartedUsingWebRTC();
  EXPECT_TRUE(frame_node->UsesWebRTC());
  EXPECT_CALL(obs, OnFrameUsesWebRTCChanged(frame_node.get()));
  frame_node->OnStoppedUsingWebRTC();
  EXPECT_FALSE(frame_node->UsesWebRTC());

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, Priority) {
  using PriorityAndReason = execution_context_priority::PriorityAndReason;

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  // By default the priority should be "lowest".
  EXPECT_EQ(base::TaskPriority::LOWEST,
            frame_node->GetPriorityAndReason().priority());

  // Changed the reason only.
  static const char kDummyReason[] = "this is a reason!";
  EXPECT_CALL(obs,
              OnPriorityAndReasonChanged(
                  frame_node.get(),
                  PriorityAndReason(base::TaskPriority::LOWEST,
                                    FrameNodeImpl::kDefaultPriorityReason)));
  frame_node->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::LOWEST, kDummyReason));
  EXPECT_EQ(PriorityAndReason(base::TaskPriority::LOWEST, kDummyReason),
            frame_node->GetPriorityAndReason());
  testing::Mock::VerifyAndClear(&obs);

  // Change the priority only.
  EXPECT_CALL(obs,
              OnPriorityAndReasonChanged(
                  frame_node.get(),
                  PriorityAndReason(base::TaskPriority::LOWEST, kDummyReason)));
  frame_node->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::HIGHEST, kDummyReason));
  EXPECT_EQ(PriorityAndReason(base::TaskPriority::HIGHEST, kDummyReason),
            frame_node->GetPriorityAndReason());
  testing::Mock::VerifyAndClear(&obs);

  // Change neither.
  frame_node->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::HIGHEST, kDummyReason));
  EXPECT_EQ(PriorityAndReason(base::TaskPriority::HIGHEST, kDummyReason),
            frame_node->GetPriorityAndReason());
  testing::Mock::VerifyAndClear(&obs);

  // Change both the priority and the reason.
  EXPECT_CALL(
      obs, OnPriorityAndReasonChanged(
               frame_node.get(),
               PriorityAndReason(base::TaskPriority::HIGHEST, kDummyReason)));
  frame_node->SetPriorityAndReason(
      PriorityAndReason(base::TaskPriority::LOWEST, nullptr));
  EXPECT_EQ(PriorityAndReason(base::TaskPriority::LOWEST, nullptr),
            frame_node->GetPriorityAndReason());
  testing::Mock::VerifyAndClear(&obs);

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, UserActivation) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  EXPECT_FALSE(frame_node->HadFormInteraction());

  EXPECT_CALL(obs, OnHadUserActivationChanged(frame_node.get()));
  frame_node->SetHadUserActivation();
  EXPECT_TRUE(frame_node->HadUserActivation());
  testing::Mock::VerifyAndClear(&obs);

  EXPECT_CALL(obs, OnHadUserActivationChanged(frame_node.get())).Times(0);
  frame_node->SetHadUserActivation();
  EXPECT_TRUE(frame_node->HadUserActivation());
  testing::Mock::VerifyAndClear(&obs);

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, FormInteractions) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  EXPECT_CALL(obs, OnHadFormInteractionChanged(frame_node.get()));
  frame_node->SetHadFormInteraction();
  EXPECT_TRUE(frame_node->HadFormInteraction());

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, UserEdits) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  EXPECT_CALL(obs, OnHadUserEditsChanged(frame_node.get()));
  frame_node->SetHadUserEdits();
  EXPECT_TRUE(frame_node->HadUserEdits());

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, IsAudible) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  EXPECT_FALSE(frame_node->IsAudible());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  EXPECT_CALL(obs, OnIsAudibleChanged(frame_node.get()));
  frame_node->SetIsAudible(true);
  EXPECT_TRUE(frame_node->IsAudible());

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, IsCapturingMediaStream) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  EXPECT_FALSE(frame_node->IsCapturingMediaStream());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  EXPECT_CALL(obs, OnIsCapturingMediaStreamChanged(frame_node.get()));
  frame_node->SetIsCapturingMediaStream(true);
  EXPECT_TRUE(frame_node->IsCapturingMediaStream());

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, ViewportIntersection) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  // A child frame node is used because the intersection with the viewport of a
  // main frame is not tracked.
  auto main_frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  auto child_frame_node =
      CreateFrameNodeAutoId(process.get(), page.get(), main_frame_node.get());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  // Initially unknown.
  EXPECT_EQ(child_frame_node->GetViewportIntersection(), std::nullopt);

  EXPECT_CALL(obs, OnViewportIntersectionChanged(child_frame_node.get()));
  child_frame_node->SetViewportIntersectionForTesting(
      ViewportIntersection::CreateNotIntersecting());
  EXPECT_FALSE(child_frame_node->GetViewportIntersection()->is_intersecting());
  EXPECT_FALSE(child_frame_node->GetViewportIntersection()
                   ->is_intersecting_large_area());

  EXPECT_CALL(obs, OnViewportIntersectionChanged(child_frame_node.get()));
  child_frame_node->SetViewportIntersectionForTesting(
      ViewportIntersection::CreateIntersecting(
          /*is_intersecting_large_area=*/true));
  EXPECT_TRUE(child_frame_node->GetViewportIntersection()->is_intersecting());
  EXPECT_TRUE(child_frame_node->GetViewportIntersection()
                  ->is_intersecting_large_area());

  EXPECT_CALL(obs, OnViewportIntersectionChanged(child_frame_node.get()));
  child_frame_node->SetViewportIntersectionForTesting(
      ViewportIntersection::CreateIntersecting(
          /*is_intersecting_large_area=*/false));
  EXPECT_TRUE(child_frame_node->GetViewportIntersection()->is_intersecting());
  EXPECT_FALSE(child_frame_node->GetViewportIntersection()
                   ->is_intersecting_large_area());

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, ViewportIntersection_IsIntersectingLargeArea) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto main_frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  // Must have a local root in another process.
  auto other_process = CreateNode<ProcessNodeImpl>();
  auto local_root = CreateFrameNodeAutoId(other_process.get(), page.get(),
                                          main_frame_node.get());

  // Set the local root to be intersecting with a large area of the viewport.
  local_root->SetViewportIntersectionForTesting(
      ViewportIntersection::CreateIntersecting(
          /*is_intersecting_large_area=*/true));
  EXPECT_TRUE(
      local_root->GetViewportIntersection()->is_intersecting_large_area());

  // Create a local child frame that intersects with the viewport.

  auto local_child =
      CreateFrameNodeAutoId(other_process.get(), page.get(), local_root.get());
  local_child->SetViewportIntersectionForTesting(
      /*is_intersecting_viewport*/ true);

  // The child inherited the `is_intersecting_large_area` bit from its parent.
  EXPECT_TRUE(
      local_child->GetViewportIntersection()->is_intersecting_large_area());

  // Make the local root intersecting with a non-large area of the viewport.
  local_root->SetViewportIntersectionForTesting(
      ViewportIntersection::CreateIntersecting(
          /*is_intersecting_large_area=*/false));
  EXPECT_FALSE(
      local_root->GetViewportIntersection()->is_intersecting_large_area());

  // The child inherited the `is_intersecting_large_area` bit from its parent.
  EXPECT_FALSE(
      local_child->GetViewportIntersection()->is_intersecting_large_area());
}

TEST_F(FrameNodeImplTest, Visibility) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kUnknown);

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  EXPECT_CALL(obs, OnFrameVisibilityChanged(frame_node.get(),
                                            FrameNode::Visibility::kUnknown));

  frame_node->SetVisibility(FrameNode::Visibility::kVisible);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kVisible);

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, FirstContentfulPaint) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs;
  graph()->AddFrameNodeObserver(&obs);

  base::TimeDelta fcp = base::Milliseconds(1364);
  EXPECT_CALL(obs, OnFirstContentfulPaint(frame_node.get(), fcp));
  frame_node->OnFirstContentfulPaint(fcp);

  graph()->RemoveFrameNodeObserver(&obs);
}

TEST_F(FrameNodeImplTest, PublicInterface) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  auto child_frame_node =
      CreateFrameNodeAutoId(process.get(), page.get(), frame_node.get());
  const FrameNode* public_frame_node = frame_node.get();

  // Simply test that the public interface impls yield the same result as their
  // private counterpart.

  EXPECT_EQ(static_cast<const FrameNode*>(frame_node->parent_frame_node()),
            public_frame_node->GetParentFrameNode());
  EXPECT_EQ(static_cast<const PageNode*>(frame_node->page_node()),
            public_frame_node->GetPageNode());
  EXPECT_EQ(static_cast<const ProcessNode*>(frame_node->process_node()),
            public_frame_node->GetProcessNode());

  auto child_frame_nodes = public_frame_node->GetChildFrameNodes();
  for (FrameNodeImpl* child : frame_node->child_frame_nodes()) {
    EXPECT_TRUE(base::Contains(child_frame_nodes, child));
  }
  EXPECT_EQ(child_frame_nodes.size(), frame_node->child_frame_nodes().size());
}

namespace {

class LenientMockPageObserver : public PageNode::ObserverDefaultImpl {
 public:
  LenientMockPageObserver() = default;
  ~LenientMockPageObserver() override = default;

  MOCK_METHOD(void,
              OnBeforePageNodeRemoved,
              (const PageNode* page_node),
              (override));

  // Note that embedder functionality is actually tested in the
  // performance_manager_browsertest.
  MOCK_METHOD(void,
              OnOpenerFrameNodeChanged,
              (const PageNode*, const FrameNode*),
              (override));
  MOCK_METHOD(void,
              OnEmbedderFrameNodeChanged,
              (const PageNode*, const FrameNode*, EmbeddingType),
              (override));
};

using MockPageObserver = ::testing::StrictMock<LenientMockPageObserver>;

}  // namespace

TEST_F(FrameNodeImplTest, PageRelationships) {
  using EmbeddingType = PageNode::EmbeddingType;

  auto process = CreateNode<ProcessNodeImpl>();
  auto pageA = CreateNode<PageNodeImpl>();
  auto frameA1 = CreateFrameNodeAutoId(process.get(), pageA.get());
  auto frameA2 =
      CreateFrameNodeAutoId(process.get(), pageA.get(), frameA1.get());
  auto pageB = CreateNode<PageNodeImpl>();
  auto frameB1 = CreateFrameNodeAutoId(process.get(), pageB.get());
  auto pageC = CreateNode<PageNodeImpl>();
  auto frameC1 = CreateFrameNodeAutoId(process.get(), pageC.get());

  // Use these to test the public APIs as well.
  const FrameNode* pframeA1 = static_cast<const FrameNode*>(frameA1.get());
  const PageNode* ppageB = static_cast<const PageNode*>(pageB.get());

  MockPageObserver obs;
  graph()->AddPageNodeObserver(&obs);

  // You can always call the pre-delete embedder clearing helper, even if you
  // have no such relationships.
  frameB1->SeverPageRelationshipsAndMaybeReparentForTesting();

  // You can't clear an embedder if you don't already have one.
  EXPECT_DCHECK_DEATH(pageB->ClearEmbedderFrameNodeAndEmbeddingType());

  // You can't be an embedder for your own frame tree.
  EXPECT_DCHECK_DEATH(pageA->SetEmbedderFrameNodeAndEmbeddingType(
      frameA1.get(), EmbeddingType::kGuestView));

  // You can't set a null embedder or an invalid embedded type.
  EXPECT_DCHECK_DEATH(pageB->SetEmbedderFrameNodeAndEmbeddingType(
      nullptr, EmbeddingType::kInvalid));
  EXPECT_DCHECK_DEATH(pageB->SetEmbedderFrameNodeAndEmbeddingType(
      frameA1.get(), EmbeddingType::kInvalid));

  EXPECT_EQ(nullptr, pageB->embedder_frame_node());
  EXPECT_EQ(nullptr, ppageB->GetEmbedderFrameNode());
  EXPECT_EQ(EmbeddingType::kInvalid, ppageB->GetEmbeddingType());
  EXPECT_TRUE(frameA1->embedded_page_nodes().empty());
  EXPECT_TRUE(pframeA1->GetEmbeddedPageNodes().empty());

  // Set an embedder relationship.
  EXPECT_CALL(obs, OnEmbedderFrameNodeChanged(pageB.get(), nullptr,
                                              EmbeddingType::kInvalid));
  pageB->SetEmbedderFrameNodeAndEmbeddingType(frameA1.get(),
                                              EmbeddingType::kGuestView);
  EXPECT_EQ(frameA1.get(), pageB->embedder_frame_node());
  EXPECT_EQ(frameA1.get(), ppageB->GetEmbedderFrameNode());
  EXPECT_EQ(EmbeddingType::kGuestView, ppageB->GetEmbeddingType());
  EXPECT_EQ(1u, frameA1->embedded_page_nodes().size());
  EXPECT_EQ(1u, pframeA1->GetEmbeddedPageNodes().size());
  EXPECT_TRUE(base::Contains(frameA1->embedded_page_nodes(), pageB.get()));
  EXPECT_TRUE(base::Contains(pframeA1->GetEmbeddedPageNodes(), pageB.get()));
  testing::Mock::VerifyAndClear(&obs);

  // Set an opener relationship.
  EXPECT_CALL(obs, OnOpenerFrameNodeChanged(pageC.get(), nullptr));
  pageC->SetOpenerFrameNode(frameA1.get());
  EXPECT_EQ(frameA1.get(), pageC->opener_frame_node());
  EXPECT_EQ(1u, frameA1->embedded_page_nodes().size());
  EXPECT_EQ(1u, frameA1->opened_page_nodes().size());
  EXPECT_TRUE(base::Contains(frameA1->embedded_page_nodes(), pageB.get()));
  testing::Mock::VerifyAndClear(&obs);

  // Manually clear the embedder relationship (initiated from the page).
  EXPECT_CALL(obs, OnEmbedderFrameNodeChanged(pageB.get(), frameA1.get(),
                                              EmbeddingType::kGuestView));
  pageB->ClearEmbedderFrameNodeAndEmbeddingType();
  EXPECT_EQ(nullptr, pageB->embedder_frame_node());
  EXPECT_EQ(EmbeddingType::kInvalid, pageB->GetEmbeddingType());
  EXPECT_EQ(frameA1.get(), pageC->opener_frame_node());
  EXPECT_TRUE(frameA1->embedded_page_nodes().empty());
  testing::Mock::VerifyAndClear(&obs);

  // Clear the opener relationship.
  EXPECT_CALL(obs, OnOpenerFrameNodeChanged(pageC.get(), frameA1.get()));
  frameA1->SeverPageRelationshipsAndMaybeReparentForTesting();
  EXPECT_EQ(nullptr, pageC->embedder_frame_node());
  EXPECT_EQ(EmbeddingType::kInvalid, pageC->GetEmbeddingType());
  EXPECT_TRUE(frameA1->opened_page_nodes().empty());
  EXPECT_TRUE(frameA1->embedded_page_nodes().empty());
  testing::Mock::VerifyAndClear(&obs);

  // Set a popup relationship on node A2.
  EXPECT_CALL(obs, OnOpenerFrameNodeChanged(pageB.get(), nullptr));
  pageB->SetOpenerFrameNode(frameA2.get());
  EXPECT_EQ(frameA2.get(), pageB->opener_frame_node());
  EXPECT_TRUE(frameA1->opened_page_nodes().empty());
  EXPECT_TRUE(frameA1->embedded_page_nodes().empty());
  EXPECT_EQ(1u, frameA2->opened_page_nodes().size());
  EXPECT_TRUE(frameA2->embedded_page_nodes().empty());
  EXPECT_TRUE(base::Contains(frameA2->opened_page_nodes(), pageB.get()));
  testing::Mock::VerifyAndClear(&obs);

  // Clear it with the helper, and expect it to be reparented to node A1.
  EXPECT_CALL(obs, OnOpenerFrameNodeChanged(pageB.get(), frameA2.get()));
  frameA2->SeverPageRelationshipsAndMaybeReparentForTesting();
  EXPECT_EQ(frameA1.get(), pageB->opener_frame_node());
  EXPECT_EQ(1u, frameA1->opened_page_nodes().size());
  EXPECT_TRUE(base::Contains(frameA1->opened_page_nodes(), pageB.get()));
  EXPECT_TRUE(frameA2->opened_page_nodes().empty());
  testing::Mock::VerifyAndClear(&obs);

  // Clear it again with the helper. This time reparenting can't happen, as it
  // was already parented to the root.
  EXPECT_CALL(obs, OnOpenerFrameNodeChanged(pageB.get(), frameA1.get()));
  frameA1->SeverPageRelationshipsAndMaybeReparentForTesting();
  EXPECT_EQ(nullptr, pageB->opener_frame_node());
  EXPECT_TRUE(frameA1->opened_page_nodes().empty());
  EXPECT_TRUE(frameA2->opened_page_nodes().empty());
  testing::Mock::VerifyAndClear(&obs);

  // verify that the embedder relationship is torn down before any node removal
  // notification arrives.
  EXPECT_CALL(obs, OnOpenerFrameNodeChanged(pageB.get(), nullptr));
  pageB->SetOpenerFrameNode(frameA2.get());
  EXPECT_EQ(frameA2.get(), pageB->opener_frame_node());
  EXPECT_TRUE(frameA1->opened_page_nodes().empty());
  EXPECT_EQ(1u, frameA2->opened_page_nodes().size());
  EXPECT_TRUE(base::Contains(frameA2->opened_page_nodes(), pageB.get()));
  testing::Mock::VerifyAndClear(&obs);

  {
    ::testing::InSequence seq;

    // These must occur in sequence.
    EXPECT_CALL(obs, OnOpenerFrameNodeChanged(pageB.get(), frameA2.get()));
    EXPECT_CALL(obs, OnBeforePageNodeRemoved(pageB.get()));
  }
  frameB1.reset();
  pageB.reset();
  testing::Mock::VerifyAndClear(&obs);

  graph()->RemovePageNodeObserver(&obs);
}

}  // namespace performance_manager
