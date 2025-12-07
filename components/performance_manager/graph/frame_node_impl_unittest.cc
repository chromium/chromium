// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/frame_node_impl.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/task_traits.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/test_support/graph/mock_frame_node_observer.h"
#include "components/performance_manager/test_support/graph/mock_page_node_observer.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "mojo/public/cpp/bindings/remote.h"
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

TEST_F(FrameNodeImplTest,
       NavigationCommitted_DifferentDocument_UrlAndOriginUpdated) {
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

TEST_F(FrameNodeImplTest,
       NavigationCommitted_DifferentDocument_PropertiesReset) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  frame_node->SetHasNonEmptyBeforeUnload(true);
  frame_node->SetNetworkAlmostIdle();
  frame_node->SetHadFormInteraction();
  frame_node->SetHadUserEdits();
  frame_node->OnStartedUsingWebRTC();
  frame_node->OnFreezingOriginTrialOptOut();

  EXPECT_TRUE(frame_node->HasNonemptyBeforeUnload());
  EXPECT_TRUE(frame_node->GetNetworkAlmostIdle());
  EXPECT_TRUE(frame_node->HadFormInteraction());
  EXPECT_TRUE(frame_node->HadUserEdits());
  EXPECT_TRUE(frame_node->UsesWebRTC());
  EXPECT_TRUE(frame_node->HasFreezingOriginTrialOptOut());

  const GURL kUrl("http://www.foo.com/");
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  frame_node->OnNavigationCommitted(
      kUrl, kOrigin, /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  EXPECT_EQ(kUrl, frame_node->GetURL());
  EXPECT_EQ(kOrigin, frame_node->GetOrigin());

  EXPECT_FALSE(frame_node->HasNonemptyBeforeUnload());
  EXPECT_FALSE(frame_node->GetNetworkAlmostIdle());
  EXPECT_FALSE(frame_node->HadFormInteraction());
  EXPECT_FALSE(frame_node->HadUserEdits());
  EXPECT_FALSE(frame_node->UsesWebRTC());
  EXPECT_FALSE(frame_node->HasFreezingOriginTrialOptOut());
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

class MockObserver : public MockFrameNodeObserver {
 public:
  explicit MockObserver(Graph* graph = nullptr) {
    // If a `graph` is passed, automatically start observing it.
    if (graph) {
      scoped_observation_.Observe(graph);
    }
  }

  void SetCreatedFrameNode(
      const FrameNode* frame_node,
      const FrameNode* pending_parent_frame_node,
      const PageNode* pending_page_node,
      const ProcessNode* pending_process_node,
      const FrameNode* pending_parent_or_outer_document_or_embedder) {
    created_frame_node_ = frame_node;
    pending_parent_frame_node_ = pending_parent_frame_node;
    pending_page_node_ = pending_page_node;
    pending_process_node_ = pending_process_node;
    pending_parent_or_outer_document_or_embedder_ =
        pending_parent_or_outer_document_or_embedder;

    // Node should be created without edges.
    EXPECT_FALSE(frame_node->GetParentFrameNode());
    EXPECT_FALSE(frame_node->GetPageNode());
    EXPECT_FALSE(frame_node->GetProcessNode());
    EXPECT_FALSE(frame_node->GetParentOrOuterDocumentOrEmbedder());
    EXPECT_TRUE(frame_node->GetChildFrameNodes().empty());
    EXPECT_TRUE(frame_node->GetOpenedPageNodes().empty());
    EXPECT_TRUE(frame_node->GetEmbeddedPageNodes().empty());
    EXPECT_TRUE(frame_node->GetChildWorkerNodes().empty());
  }

  void TestCreatedFrameNode(const FrameNode* frame_node) {
    EXPECT_EQ(created_frame_node_, frame_node);
    EXPECT_EQ(pending_parent_frame_node_, frame_node->GetParentFrameNode());
    EXPECT_EQ(pending_page_node_, frame_node->GetPageNode());
    EXPECT_EQ(pending_process_node_, frame_node->GetProcessNode());
    EXPECT_EQ(pending_parent_or_outer_document_or_embedder_,
              frame_node->GetParentOrOuterDocumentOrEmbedder());

    // Avoid dangling pointers.
    pending_parent_frame_node_ = nullptr;
    pending_page_node_ = nullptr;
    pending_process_node_ = nullptr;
    pending_parent_or_outer_document_or_embedder_ = nullptr;
  }

  const FrameNode* created_frame_node() { return created_frame_node_; }

 private:
  base::ScopedObservation<Graph, FrameNodeObserver> scoped_observation_{this};
  raw_ptr<const FrameNode, DanglingUntriaged> created_frame_node_ = nullptr;
  raw_ptr<const FrameNode> pending_parent_frame_node_ = nullptr;
  raw_ptr<const PageNode> pending_page_node_ = nullptr;
  raw_ptr<const ProcessNode> pending_process_node_ = nullptr;
  raw_ptr<const FrameNode> pending_parent_or_outer_document_or_embedder_ =
      nullptr;
};

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::WithArg;

// Called repeatedly from a mock FrameNodeObserver. At each call it tries to set
// a property on the node depending on the last action set with `set_action()`,
// unless the action is kDoNothing.
class NodePropertySetter {
 public:
  NodePropertySetter() = default;
  ~NodePropertySetter() = default;

  NodePropertySetter(const NodePropertySetter&) = delete;
  NodePropertySetter& operator=(const NodePropertySetter&) = delete;

  enum class Action {
    kUndefined,
    kSetAndNotify,
    kSetWithoutNotify,
    kDoNothing,
  };

  void set_action(Action action) { action_ = action; }

  void MaybeSetProperty(const FrameNode* frame_node) {
    auto* impl = FrameNodeImpl::FromNode(frame_node);
    switch (action_) {
      case Action::kSetAndNotify:
        // Property that notifies.
        impl->SetIsAdFrame(true);
        break;
      case Action::kSetWithoutNotify:
        // Property that doesn't notify.
        impl->SetInitialVisibility(FrameNode::Visibility::kVisible);
        break;
      case Action::kDoNothing:
        break;
      case Action::kUndefined:
        FAIL() << "NodePropertySetter::set_action() wasn't called";
    }
  }

 private:
  Action action_ = Action::kUndefined;
};

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
  EXPECT_CALL(head_obs, OnBeforeFrameNodeAdded(_, _, _, _, _))
      .WillOnce(InvokeWithoutArgs([&] {
        graph()->RemoveFrameNodeObserver(&head_obs);
        graph()->RemoveFrameNodeObserver(&tail_obs);
      }));
  // `tail_obs` should not be notified as it was removed.
  EXPECT_CALL(tail_obs, OnBeforeFrameNodeAdded(_, _, _, _, _)).Times(0);

  // Create a frame node and expect a matching call to both "OnBeforeFrameNodeAdded" and
  // "OnFrameNodeAdded".
  {
    InSequence seq;
    EXPECT_CALL(obs, OnBeforeFrameNodeAdded(_, _, _, _, _))
        .WillOnce(Invoke(&obs, &MockObserver::SetCreatedFrameNode));
    EXPECT_CALL(obs, OnFrameNodeAdded(_))
        .WillOnce(Invoke(&obs, &MockObserver::TestCreatedFrameNode));
  }
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

  // Release the frame node and expect a call to both "OnBeforeFrameNodeRemoved"
  // and "OnFrameNodeRemoved".
  const FrameNode* saved_parent_frame_node = nullptr;
  const PageNode* saved_page_node = nullptr;
  const ProcessNode* saved_process_node = nullptr;
  const FrameNode* saved_parent_or_outer_document_or_embedder = nullptr;
  {
    InSequence seq;
    EXPECT_CALL(obs, OnBeforeFrameNodeRemoved(raw_frame_node))
        .WillOnce([&](const FrameNode* frame_node) {
          // Node should still be in graph.
          saved_parent_frame_node = frame_node->GetParentFrameNode();
          saved_page_node = frame_node->GetPageNode();
          saved_process_node = frame_node->GetProcessNode();
          saved_parent_or_outer_document_or_embedder =
              frame_node->GetParentOrOuterDocumentOrEmbedder();
          // Frame had no parent, so only page and process are set.
          EXPECT_TRUE(saved_page_node);
          EXPECT_TRUE(saved_process_node);
        });
    EXPECT_CALL(obs, OnFrameNodeRemoved(raw_frame_node, _, _, _, _))
        .WillOnce([&](const FrameNode* frame_node,
                      const FrameNode* previous_parent_frame_node,
                      const PageNode* previous_page_node,
                      const ProcessNode* previous_process_node,
                      const FrameNode*
                          previous_parent_or_outer_document_or_embedder) {
          EXPECT_EQ(saved_parent_frame_node, previous_parent_frame_node);
          EXPECT_EQ(saved_page_node, previous_page_node);
          EXPECT_EQ(saved_process_node, previous_process_node);
          EXPECT_EQ(saved_parent_or_outer_document_or_embedder,
                    previous_parent_or_outer_document_or_embedder);
          EXPECT_FALSE(frame_node->GetParentFrameNode());
          EXPECT_FALSE(frame_node->GetPageNode());
          EXPECT_FALSE(frame_node->GetProcessNode());
          EXPECT_FALSE(frame_node->GetParentOrOuterDocumentOrEmbedder());
        });
  }
  frame_node.reset();
  testing::Mock::VerifyAndClear(&obs);

  graph()->RemoveFrameNodeObserver(&obs);
}

// This is an end-to-end test of the logic that explodes when recursive
// notifications are dispatched during node creation and removal. There are
// other tests of the individual pieces of logic in NodeBase and
// ObservedProperty.
TEST_F(FrameNodeImplDeathTest, SetPropertyDuringNodeCreation) {
  MockObserver obs(graph());

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();

  // Modifying a property during node addition should explode, whether or not
  // it notifies.
  NodePropertySetter property_setter;
  {
    InSequence seq;
    EXPECT_CALL(obs, OnBeforeFrameNodeAdded(_, _, _, _, _));
    EXPECT_CALL(obs, OnFrameNodeAdded(_))
        .WillOnce(
            Invoke(&property_setter, &NodePropertySetter::MaybeSetProperty));
  }

  // Every EXPECT_DCHECK_DEATH forks the test, with one branch executing the
  // expectation and dying, and the other continuing. So the mock expectation
  // is installed once and invoked multiple times (once in each fork). The last
  // invocation must satisfy the expectation without crashing.
  property_setter.set_action(NodePropertySetter::Action::kSetAndNotify);
  EXPECT_DCHECK_DEATH(auto frame =
                          CreateFrameNodeAutoId(process.get(), page.get()));
  property_setter.set_action(NodePropertySetter::Action::kSetWithoutNotify);
  EXPECT_DCHECK_DEATH(auto frame =
                          CreateFrameNodeAutoId(process.get(), page.get()));
  property_setter.set_action(NodePropertySetter::Action::kDoNothing);
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());

  // Modifying a property during node removal should also explode.
  {
    InSequence seq;
    EXPECT_CALL(obs, OnBeforeFrameNodeRemoved(_))
        .WillOnce(
            Invoke(&property_setter, &NodePropertySetter::MaybeSetProperty));
    EXPECT_CALL(obs, OnFrameNodeRemoved(_, _, _, _, _));
  }

  property_setter.set_action(NodePropertySetter::Action::kSetAndNotify);
  EXPECT_DCHECK_DEATH(frame.reset());
  property_setter.set_action(NodePropertySetter::Action::kSetWithoutNotify);
  EXPECT_DCHECK_DEATH(frame.reset());
  property_setter.set_action(NodePropertySetter::Action::kDoNothing);
  frame.reset();
}

TEST_F(FrameNodeImplDeathTest, SetPropertyBeforeNodeAdded) {
  MockObserver obs(graph());

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();

  // Modifying a property that notifies before node addition should explode.
  NodePropertySetter property_setter;
  {
    InSequence seq;
    EXPECT_CALL(obs, OnBeforeFrameNodeAdded(_, _, _, _, _))
        .WillOnce(WithArg<0>(
            Invoke(&property_setter, &NodePropertySetter::MaybeSetProperty)));
    EXPECT_CALL(obs, OnFrameNodeAdded(_));
  }

  // Every EXPECT_DCHECK_DEATH forks the test, with one branch executing the
  // expectation and dying, and the other continuing. So the mock expectation
  // is installed once and invoked multiple times (once in each fork). The last
  // invocation must satisfy the expectation without crashing.
  property_setter.set_action(NodePropertySetter::Action::kSetAndNotify);
  EXPECT_DCHECK_DEATH(auto frame =
                          CreateFrameNodeAutoId(process.get(), page.get()));
  property_setter.set_action(NodePropertySetter::Action::kSetWithoutNotify);
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());

  // Modifying a property after node removal should explode, whether or not it
  // notifies.
  {
    InSequence seq;
    EXPECT_CALL(obs, OnBeforeFrameNodeRemoved(_));
    EXPECT_CALL(obs, OnFrameNodeRemoved(_, _, _, _, _))
        .WillOnce(WithArg<0>(
            Invoke(&property_setter, &NodePropertySetter::MaybeSetProperty)));
  }
  property_setter.set_action(NodePropertySetter::Action::kSetAndNotify);
  EXPECT_DCHECK_DEATH(frame.reset());
  property_setter.set_action(NodePropertySetter::Action::kSetWithoutNotify);
  EXPECT_DCHECK_DEATH(frame.reset());
  property_setter.set_action(NodePropertySetter::Action::kDoNothing);
  frame.reset();
}

TEST_F(FrameNodeImplTest, IsAdFrame) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs(graph());

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
}

TEST_F(FrameNodeImplTest, IsActive) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  // is_active is true by default from CreateFrameNodeAutoId.
  EXPECT_TRUE(frame_node->IsActive());

  frame_node->SetIsActive(false);
  EXPECT_FALSE(frame_node->IsActive());
  frame_node->SetIsActive(true);
  EXPECT_TRUE(frame_node->IsActive());
}

TEST_F(FrameNodeImplTest, IsHoldingWebLock) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs(graph());

  EXPECT_FALSE(frame_node->IsHoldingWebLock());
  EXPECT_CALL(obs, OnFrameIsHoldingWebLockChanged(frame_node.get()));
  frame_node->SetIsHoldingWebLock(true);
  EXPECT_TRUE(frame_node->IsHoldingWebLock());
  EXPECT_CALL(obs, OnFrameIsHoldingWebLockChanged(frame_node.get()));
  frame_node->SetIsHoldingWebLock(false);
  EXPECT_FALSE(frame_node->IsHoldingWebLock());
}

TEST_F(FrameNodeImplTest, IsHoldingBlockingIndexedDBLock) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs(graph());

  EXPECT_CALL(obs,
              OnFrameIsHoldingBlockingIndexedDBLockChanged(frame_node.get()));
  frame_node->SetIsHoldingBlockingIndexedDBLock(true);
  EXPECT_TRUE(frame_node->IsHoldingBlockingIndexedDBLock());
  EXPECT_CALL(obs,
              OnFrameIsHoldingBlockingIndexedDBLockChanged(frame_node.get()));
  frame_node->SetIsHoldingBlockingIndexedDBLock(false);
  EXPECT_FALSE(frame_node->IsHoldingBlockingIndexedDBLock());
}

TEST_F(FrameNodeImplTest, UsesWebRTC) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs(graph());

  EXPECT_CALL(obs, OnFrameUsesWebRTCChanged(frame_node.get()));
  frame_node->OnStartedUsingWebRTC();
  EXPECT_TRUE(frame_node->UsesWebRTC());
  EXPECT_CALL(obs, OnFrameUsesWebRTCChanged(frame_node.get()));
  frame_node->OnStoppedUsingWebRTC();
  EXPECT_FALSE(frame_node->UsesWebRTC());
}

TEST_F(FrameNodeImplTest, Priority) {
  using PriorityAndReason = execution_context_priority::PriorityAndReason;

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs(graph());

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
}

TEST_F(FrameNodeImplTest, UserActivation) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs(graph());

  EXPECT_FALSE(frame_node->HadFormInteraction());

  EXPECT_CALL(obs, OnHadUserActivationChanged(frame_node.get()));
  frame_node->SetHadUserActivation();
  EXPECT_TRUE(frame_node->HadUserActivation());
  testing::Mock::VerifyAndClear(&obs);

  EXPECT_CALL(obs, OnHadUserActivationChanged(frame_node.get())).Times(0);
  frame_node->SetHadUserActivation();
  EXPECT_TRUE(frame_node->HadUserActivation());
  testing::Mock::VerifyAndClear(&obs);
}

TEST_F(FrameNodeImplTest, FormInteractions) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs(graph());

  EXPECT_CALL(obs, OnHadFormInteractionChanged(frame_node.get()));
  frame_node->SetHadFormInteraction();
  EXPECT_TRUE(frame_node->HadFormInteraction());
}

TEST_F(FrameNodeImplTest, UserEdits) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs(graph());

  EXPECT_CALL(obs, OnHadUserEditsChanged(frame_node.get()));
  frame_node->SetHadUserEdits();
  EXPECT_TRUE(frame_node->HadUserEdits());
}

TEST_F(FrameNodeImplTest, IsAudible) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  EXPECT_FALSE(frame_node->IsAudible());

  MockObserver obs(graph());

  EXPECT_CALL(obs, OnIsAudibleChanged(frame_node.get()));
  frame_node->SetIsAudible(true);
  EXPECT_TRUE(frame_node->IsAudible());
}

TEST_F(FrameNodeImplTest, IsCapturingMediaStream) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  EXPECT_FALSE(frame_node->IsCapturingMediaStream());

  MockObserver obs(graph());

  EXPECT_CALL(obs, OnIsCapturingMediaStreamChanged(frame_node.get()));
  frame_node->SetIsCapturingMediaStream(true);
  EXPECT_TRUE(frame_node->IsCapturingMediaStream());
}

TEST_F(FrameNodeImplTest, HasFreezingOriginTrialOptOut) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  EXPECT_FALSE(frame_node->HasFreezingOriginTrialOptOut());

  MockObserver obs(graph());

  EXPECT_CALL(obs,
              OnFrameHasFreezingOriginTrialOptOutChanged(frame_node.get()));
  frame_node->OnFreezingOriginTrialOptOut();
  EXPECT_TRUE(frame_node->HasFreezingOriginTrialOptOut());
}

TEST_F(FrameNodeImplTest, ViewportIntersection) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  // A child frame node is used because the intersection with the viewport of a
  // main frame is not tracked.
  auto main_frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  auto child_frame_node =
      CreateFrameNodeAutoId(process.get(), page.get(), main_frame_node.get());

  MockObserver obs(graph());

  // Ignore OnIsIntersectingLargeArea notifications.
  EXPECT_CALL(obs, OnIsIntersectingLargeAreaChanged(child_frame_node.get()))
      .Times(AnyNumber());

  // Initially unknown.
  EXPECT_EQ(child_frame_node->GetViewportIntersection(),
            ViewportIntersection::kUnknown);

  EXPECT_CALL(obs, OnViewportIntersectionChanged(child_frame_node.get()));
  child_frame_node->SetViewportIntersection(
      ViewportIntersection::kNotIntersecting);
  EXPECT_EQ(child_frame_node->GetViewportIntersection(),
            ViewportIntersection::kNotIntersecting);

  EXPECT_CALL(obs, OnViewportIntersectionChanged(child_frame_node.get()));
  child_frame_node->SetViewportIntersection(
      ViewportIntersection::kIntersecting);
  EXPECT_EQ(child_frame_node->GetViewportIntersection(),
            ViewportIntersection::kIntersecting);
}

TEST_F(FrameNodeImplTest, IsIntersectingLargeArea) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto main_frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  // Must have a local root in another process.
  auto other_process = CreateNode<ProcessNodeImpl>();
  auto local_root = CreateFrameNodeAutoId(other_process.get(), page.get(),
                                          main_frame_node.get());

  MockObserver obs(graph());

  // Ignore OnViewportIntersectionChanged notifications.
  EXPECT_CALL(obs, OnViewportIntersectionChanged(local_root.get()))
      .Times(AnyNumber());

  // By default, a frame is assumed to be intersecting with a large area of the
  // viewport.
  EXPECT_TRUE(local_root->IsIntersectingLargeArea());

  EXPECT_CALL(obs, OnIsIntersectingLargeAreaChanged(local_root.get()));
  local_root->SetIsIntersectingLargeArea(false);
  EXPECT_FALSE(local_root->IsIntersectingLargeArea());

  EXPECT_CALL(obs, OnIsIntersectingLargeAreaChanged(local_root.get()));
  local_root->SetIsIntersectingLargeArea(true);
  EXPECT_TRUE(local_root->IsIntersectingLargeArea());

  // IsIntersectingLargeArea() is false if GetViewportIntersection is
  // kNotIntersecting.
  EXPECT_CALL(obs, OnIsIntersectingLargeAreaChanged(local_root.get()));
  local_root->SetViewportIntersection(ViewportIntersection::kNotIntersecting);
  EXPECT_FALSE(local_root->IsIntersectingLargeArea());

  // Toggling IsIntersectingLargeArea() while the viewport intersection is
  // kNotIntersecting doesn't affect its value.
  local_root->SetIsIntersectingLargeArea(false);
  EXPECT_FALSE(local_root->IsIntersectingLargeArea());
  local_root->SetIsIntersectingLargeArea(true);
  EXPECT_FALSE(local_root->IsIntersectingLargeArea());

  // Change the viewport intersection to kIntersecting and observe the property
  // change.
  EXPECT_CALL(obs, OnIsIntersectingLargeAreaChanged(local_root.get()));
  local_root->SetViewportIntersection(ViewportIntersection::kIntersecting);
  EXPECT_TRUE(local_root->IsIntersectingLargeArea());
}

TEST_F(FrameNodeImplTest, Visibility) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kUnknown);

  MockObserver obs(graph());

  EXPECT_CALL(obs, OnFrameVisibilityChanged(frame_node.get(),
                                            FrameNode::Visibility::kUnknown));

  frame_node->SetVisibility(FrameNode::Visibility::kVisible);
  EXPECT_EQ(frame_node->GetVisibility(), FrameNode::Visibility::kVisible);
}

TEST_F(FrameNodeImplTest, FirstContentfulPaint) {
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process.get(), page.get());

  MockObserver obs(graph());

  base::TimeDelta fcp = base::Milliseconds(1364);
  EXPECT_CALL(obs, OnFirstContentfulPaint(frame_node.get(), fcp));
  frame_node->OnFirstContentfulPaint(fcp);
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

TEST_F(FrameNodeImplTest, PageRelationships) {
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

  MockPageNodeObserver obs;
  base::ScopedObservation<Graph, PageNodeObserver> scoped_observation(&obs);
  scoped_observation.Observe(graph());

  // You can always call the pre-delete embedder clearing helper, even if you
  // have no such relationships.
  frameB1->SeverPageRelationshipsAndMaybeReparentForTesting();

  // You can't clear an embedder if you don't already have one.
  EXPECT_DCHECK_DEATH(pageB->ClearEmbedderFrameNode());

  // You can't be an embedder for your own frame tree.
  EXPECT_DCHECK_DEATH(pageA->SetEmbedderFrameNode(frameA1.get()));

  // You can't set a null embedder.
  EXPECT_DCHECK_DEATH(pageB->SetEmbedderFrameNode(nullptr));

  EXPECT_EQ(nullptr, pageB->embedder_frame_node());
  EXPECT_EQ(nullptr, ppageB->GetEmbedderFrameNode());
  EXPECT_TRUE(frameA1->embedded_page_nodes().empty());
  EXPECT_TRUE(pframeA1->GetEmbeddedPageNodes().empty());

  // Set an embedder relationship.
  EXPECT_CALL(obs, OnEmbedderFrameNodeChanged(pageB.get(), nullptr));
  pageB->SetEmbedderFrameNode(frameA1.get());
  EXPECT_EQ(frameA1.get(), pageB->embedder_frame_node());
  EXPECT_EQ(frameA1.get(), ppageB->GetEmbedderFrameNode());
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
  EXPECT_CALL(obs, OnEmbedderFrameNodeChanged(pageB.get(), frameA1.get()));
  pageB->ClearEmbedderFrameNode();
  EXPECT_EQ(nullptr, pageB->embedder_frame_node());
  EXPECT_EQ(frameA1.get(), pageC->opener_frame_node());
  EXPECT_TRUE(frameA1->embedded_page_nodes().empty());
  testing::Mock::VerifyAndClear(&obs);

  // Clear the opener relationship.
  EXPECT_CALL(obs, OnOpenerFrameNodeChanged(pageC.get(), frameA1.get()));
  frameA1->SeverPageRelationshipsAndMaybeReparentForTesting();
  EXPECT_EQ(nullptr, pageC->embedder_frame_node());
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
    InSequence seq;

    // These must occur in sequence.
    EXPECT_CALL(obs, OnOpenerFrameNodeChanged(pageB.get(), frameA2.get()));
    EXPECT_CALL(obs, OnBeforePageNodeRemoved(pageB.get()));
    EXPECT_CALL(obs, OnPageNodeRemoved(pageB.get()));
  }
  frameB1.reset();
  pageB.reset();
  testing::Mock::VerifyAndClear(&obs);
}

// Regression test for crbug.com/391723297.
TEST_F(FrameNodeImplTest, ResetCoordinationUnitReceiverOnDiscard) {
  // Create a frame tree with two child frame nodes.
  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>();
  auto main_frame_node = CreateFrameNodeAutoId(process.get(), page.get());
  auto child_frame_node =
      CreateFrameNodeAutoId(process.get(), page.get(), main_frame_node.get());

  // Bind remotes to each frame.
  mojo::Remote<mojom::DocumentCoordinationUnit> main_frame_remote;
  main_frame_node->Bind(main_frame_remote.BindNewPipeAndPassReceiver());
  mojo::Remote<mojom::DocumentCoordinationUnit> child_frame_remote;
  child_frame_node->Bind(child_frame_remote.BindNewPipeAndPassReceiver());

  // Assert the remotes are connected to the frame receivers.
  EXPECT_TRUE(main_frame_remote.is_connected());
  EXPECT_TRUE(child_frame_remote.is_connected());

  // Notify a discard for the primary page node.
  base::RunLoop main_frame_run_loop;
  base::RunLoop child_frame_run_loop;
  main_frame_remote.set_disconnect_handler(main_frame_run_loop.QuitClosure());
  child_frame_remote.set_disconnect_handler(child_frame_run_loop.QuitClosure());
  page->OnAboutToBeDiscarded(page->GetWeakPtr());

  // The connection should have been reset.
  main_frame_run_loop.Run();
  child_frame_run_loop.Run();
  EXPECT_FALSE(main_frame_remote.is_connected());
  EXPECT_FALSE(child_frame_remote.is_connected());
}

}  // namespace performance_manager
