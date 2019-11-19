// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/flat_map.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
#include "components/ui_devtools/css_agent.h"
#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/viz/dom_agent_viz.h"
#include "components/ui_devtools/viz/frame_sink_element.h"
#include "components/ui_devtools/viz/overlay_agent_viz.h"
#include "components/ui_devtools/viz/surface_element.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/surfaces/surface_reference.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace ui_devtools {
namespace {

constexpr viz::FrameSinkId kFrameSink1(1, 0);
constexpr viz::FrameSinkId kFrameSink2(2, 0);
constexpr viz::FrameSinkId kFrameSink3(3, 0);

bool HasAttributeWithValue(const std::string& attribute,
                           const std::string& value,
                           protocol::DOM::Node* node) {
  if (!node->hasAttributes())
    return false;
  protocol::Array<std::string>* attributes = node->getAttributes(nullptr);
  for (size_t i = 0; i < attributes->size() - 1; i += 2) {
    if ((*attributes)[i] == attribute) {
      return (*attributes)[i + 1] == value;
    }
  }
  return false;
}

// Recursively search for a node with an attribute and a matching value.
protocol::DOM::Node* FindNodeByAttribute(const std::string& attribute,
                                         const std::string& value,
                                         protocol::DOM::Node* root) {
  if (HasAttributeWithValue(attribute, value, root))
    return root;

  protocol::Array<protocol::DOM::Node>* children = root->getChildren(nullptr);
  for (size_t i = 0; i < children->size(); ++i) {
    protocol::DOM::Node* node =
        FindNodeByAttribute(attribute, value, (*children)[i].get());
    if (node)
      return node;
  }
  return nullptr;
}

protocol::DOM::Node* FindFrameSinkNode(const viz::FrameSinkId& frame_sink_id,
                                       protocol::DOM::Node* root) {
  return FindNodeByAttribute("FrameSinkId", frame_sink_id.ToString(), root);
}

protocol::DOM::Node* FindSurfaceNode(const viz::SurfaceId& surface_id,
                                     protocol::DOM::Node* root) {
  return FindNodeByAttribute("SurfaceId", surface_id.ToString(), root);
}

}  // namespace

class VizDevToolsTest : public PlatformTest {
 public:
  VizDevToolsTest() = default;
  ~VizDevToolsTest() override = default;

  void SetUp() override {
    frontend_channel_ = std::make_unique<FakeFrontendChannel>();
    uber_dispatcher_ =
        std::make_unique<protocol::UberDispatcher>(frontend_channel_.get());
    manager_ =
        std::make_unique<viz::FrameSinkManagerImpl>(&shared_bitmap_manager_);
    dom_agent_ = std::make_unique<DOMAgentViz>(frame_sink_manager());
    dom_agent_->Init(uber_dispatcher_.get());
    css_agent_ = std::make_unique<CSSAgent>(dom_agent_.get());
    css_agent_->Init(uber_dispatcher_.get());
    css_agent_->enable();
    overlay_agent_ = std::make_unique<OverlayAgentViz>(dom_agent_.get());
    overlay_agent_->Init(uber_dispatcher_.get());
    overlay_agent_->enable();
  }

  void TearDown() override {
    root_.reset();
    dom_agent_->disable();
    supports_.clear();
    overlay_agent_.reset();
    css_agent_.reset();
    dom_agent_.reset();
    manager_.reset();
    uber_dispatcher_.reset();
    frontend_channel_.reset();
  }

  void ExpectChildNodeInserted(int parent_id,
                               int prev_sibling_id,
                               int expected_count = 1) {
    int count = frontend_channel()->CountProtocolNotificationMessageStartsWith(
        base::StringPrintf("{\"method\":\"DOM.childNodeInserted\",\"params\":{"
                           "\"parentNodeId\":%d,\"previousNodeId\":%d,",
                           parent_id, prev_sibling_id));
    EXPECT_EQ(expected_count, count);
  }

  void ExpectChildNodeRemoved(int parent_id,
                              int node_id,
                              int expected_count = 1) {
    int count = frontend_channel()->CountProtocolNotificationMessage(
        base::StringPrintf("{\"method\":\"DOM.childNodeRemoved\",\"params\":{"
                           "\"parentNodeId\":%d,\"nodeId\":%d}}",
                           parent_id, node_id));
    EXPECT_EQ(expected_count, count);
  }

  void RegisterFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
    frame_sink_manager()->RegisterFrameSinkId(frame_sink_id, true);
  }

  void InvalidateFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
    frame_sink_manager()->InvalidateFrameSinkId(frame_sink_id);
  }

  void AddSurfaceReference(const viz::SurfaceId& parent_id,
                           const viz::SurfaceId& child_id) {
    surface_manager()->AddSurfaceReferences(
        {viz::SurfaceReference(parent_id, child_id)});
  }

  void RemoveSurfaceReference(const viz::SurfaceId& parent_id,
                              const viz::SurfaceId& child_id) {
    surface_manager()->RemoveSurfaceReferences(
        {viz::SurfaceReference(parent_id, child_id)});
  }

  // Creates a new Surface with the provided |frame_sink_id| and |parent_id|.
  // Will register and create a CompositorFrameSinkSupport for |frame_sink_id|
  // if necessary.
  viz::SurfaceId CreateFrameSinkAndSurface(
      const viz::FrameSinkId& frame_sink_id,
      uint32_t parent_id) {
    viz::LocalSurfaceId local_surface_id(parent_id,
                                         base::UnguessableToken::Create());
    // Ensure that a CompositorFrameSinkSupport exists for this frame sink.
    auto& support = supports_[frame_sink_id];
    if (!support) {
      frame_sink_manager()->RegisterFrameSinkId(frame_sink_id,
                                                /*report_activation=*/true);
      support = std::make_unique<viz::CompositorFrameSinkSupport>(
          /*client=*/nullptr, frame_sink_manager(), frame_sink_id,
          /*is_root=*/false, /*needs_sync_points=*/true);
    }
    viz::CompositorFrame frame = viz::MakeDefaultCompositorFrame();
    gfx::Size size = frame.size_in_pixels();
    support->SubmitCompositorFrame(local_surface_id, std::move(frame));
    // The surface isn't added to viz devtools yet, OnFirstSurfaceActivation
    // needs to be called.
    viz::SurfaceId surface_id(frame_sink_id, local_surface_id);
    viz::SurfaceInfo surface_info(surface_id, 1.f, size);
    surface_manager()->FirstSurfaceActivation(surface_info);
    return surface_id;
  }

  // Destroy Surface with |surface_id|, and garbage collect it.
  void DestroySurface(const viz::SurfaceId& surface_id) {
    auto support_iter = supports_.find(surface_id.frame_sink_id());
    EXPECT_NE(support_iter, supports_.end());
    support_iter->second->EvictSurface(surface_id.local_surface_id());
    surface_manager()->GarbageCollectSurfaces();
  }

  // Build the document tree, and begin listening for updates. The document
  // stored in |root_| is a snapshot and doesn't change when updates are sent
  // to |frontend_channel_|.
  void BuildDocument() {
    dom_agent()->disable();
    dom_agent()->getDocument(&root_);
    dom_agent()->enable();
  }

  DOMAgentViz* dom_agent() { return dom_agent_.get(); }
  FakeFrontendChannel* frontend_channel() { return frontend_channel_.get(); }
  viz::FrameSinkManagerImpl* frame_sink_manager() { return manager_.get(); }
  viz::SurfaceManager* surface_manager() { return manager_->surface_manager(); }
  protocol::DOM::Node* root() { return root_.get(); }

 private:
  viz::TestSharedBitmapManager shared_bitmap_manager_;
  std::unique_ptr<FakeFrontendChannel> frontend_channel_;
  std::unique_ptr<protocol::UberDispatcher> uber_dispatcher_;
  std::unique_ptr<viz::FrameSinkManagerImpl> manager_;
  std::unique_ptr<DOMAgentViz> dom_agent_;
  std::unique_ptr<CSSAgent> css_agent_;
  std::unique_ptr<OverlayAgentViz> overlay_agent_;

  std::unique_ptr<protocol::DOM::Node> root_;
  base::flat_map<viz::FrameSinkId,
                 std::unique_ptr<viz::CompositorFrameSinkSupport>>
      supports_;

  DISALLOW_COPY_AND_ASSIGN(VizDevToolsTest);
};

// Verify that registering a FrameSinkId creates a node.
TEST_F(VizDevToolsTest, FrameSinkRegistered) {
  BuildDocument();

  RegisterFrameSinkId(kFrameSink1);

  ExpectChildNodeInserted(dom_agent()->element_root()->node_id(), 0);
}

// Verify that invalidating a FrameSinkId removes a node.
TEST_F(VizDevToolsTest, FrameSinkInvalidated) {
  RegisterFrameSinkId(kFrameSink1);

  BuildDocument();

  InvalidateFrameSinkId(kFrameSink1);

  protocol::DOM::Node* frame_sink_node = FindFrameSinkNode(kFrameSink1, root());
  ExpectChildNodeRemoved(dom_agent()->element_root()->node_id(),
                         frame_sink_node->getNodeId());
}

// Verify that registering a FrameSink hierarchy moves a node to its new parent.
TEST_F(VizDevToolsTest, FrameSinkHierarchyRegistered) {
  RegisterFrameSinkId(kFrameSink1);
  RegisterFrameSinkId(kFrameSink2);

  BuildDocument();

  frame_sink_manager()->RegisterFrameSinkHierarchy(kFrameSink1, kFrameSink2);

  protocol::DOM::Node* parent_node = FindFrameSinkNode(kFrameSink1, root());
  protocol::DOM::Node* child_node = FindFrameSinkNode(kFrameSink2, root());
  ExpectChildNodeRemoved(dom_agent()->element_root()->node_id(),
                         child_node->getNodeId());
  ExpectChildNodeInserted(parent_node->getNodeId(), 0);
}

// Verify that unregistering a FrameSink hierarchy moves a node to the root
// element.
TEST_F(VizDevToolsTest, FrameSinkHierarchyUnregistered) {
  RegisterFrameSinkId(kFrameSink1);
  RegisterFrameSinkId(kFrameSink2);
  frame_sink_manager()->RegisterFrameSinkHierarchy(kFrameSink1, kFrameSink2);

  BuildDocument();

  frame_sink_manager()->UnregisterFrameSinkHierarchy(kFrameSink1, kFrameSink2);

  protocol::DOM::Node* parent_node = FindFrameSinkNode(kFrameSink1, root());
  protocol::DOM::Node* child_node = FindFrameSinkNode(kFrameSink2, parent_node);
  ExpectChildNodeRemoved(parent_node->getNodeId(), child_node->getNodeId());
  ExpectChildNodeInserted(dom_agent()->element_root()->node_id(),
                          parent_node->getNodeId());
}

// Verify that the initial tree at viz devtools startup is correct, including an
// existing hierarchy, and a detached frame sink with no parent.
TEST_F(VizDevToolsTest, InitialFrameSinkHierarchy) {
  RegisterFrameSinkId(kFrameSink1);
  RegisterFrameSinkId(kFrameSink2);
  RegisterFrameSinkId(kFrameSink3);
  frame_sink_manager()->RegisterFrameSinkHierarchy(kFrameSink1, kFrameSink2);

  BuildDocument();

  protocol::DOM::Node* node1 = FindFrameSinkNode(kFrameSink1, root());
  protocol::DOM::Node* node2 = FindFrameSinkNode(kFrameSink2, node1);
  protocol::DOM::Node* node3 = FindFrameSinkNode(kFrameSink3, root());

  // The first and third frame sinks are children of the root element.
  EXPECT_EQ(node1, (*(root()->getChildren(nullptr)))[0].get());
  EXPECT_EQ(node2, (*(node1->getChildren(nullptr)))[0].get());
  EXPECT_EQ(node3, (*(root()->getChildren(nullptr)))[1].get());
}

// Verify that FrameSinkElements are inserted into the tree according to the
// ordering of their FrameSinkIds.
TEST_F(VizDevToolsTest, FrameSinkElementOrdering) {
  RegisterFrameSinkId(kFrameSink2);

  BuildDocument();

  protocol::DOM::Node* frame_sink_node = FindFrameSinkNode(kFrameSink2, root());

  // Create a frame sink element before the existing frame sink.
  RegisterFrameSinkId(kFrameSink1);
  ExpectChildNodeInserted(dom_agent()->element_root()->node_id(), 0);

  // Create a frame sink element after the existing frame sink.
  RegisterFrameSinkId(kFrameSink3);
  ExpectChildNodeInserted(dom_agent()->element_root()->node_id(),
                          frame_sink_node->getNodeId());
}

// Verify that creating a surface and adding a reference to the root surface
// creates a node attached to the root surface node.
TEST_F(VizDevToolsTest, SurfaceCreated) {
  BuildDocument();

  viz::SurfaceId id1 = CreateFrameSinkAndSurface(kFrameSink1, 1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), id1);

  ExpectChildNodeInserted(dom_agent()->GetRootSurfaceElement()->node_id(), 0);
}

// Verify that destroying a surface removes a node from the root surface node.
TEST_F(VizDevToolsTest, SurfaceDestroyed) {
  viz::SurfaceId id1 = CreateFrameSinkAndSurface(kFrameSink1, 1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), id1);

  BuildDocument();

  RemoveSurfaceReference(surface_manager()->GetRootSurfaceId(), id1);
  DestroySurface(id1);

  protocol::DOM::Node* surface_node = FindSurfaceNode(id1, root());
  ExpectChildNodeRemoved(dom_agent()->GetRootSurfaceElement()->node_id(),
                         surface_node->getNodeId());
}

// Verify that adding a surface reference moves a node from the root surface to
// the new parent.
TEST_F(VizDevToolsTest, SurfaceReferenceAdded) {
  viz::SurfaceId id1 = CreateFrameSinkAndSurface(kFrameSink1, 1);
  viz::SurfaceId id2 = CreateFrameSinkAndSurface(kFrameSink2, 1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), id1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), id2);

  BuildDocument();

  RemoveSurfaceReference(surface_manager()->GetRootSurfaceId(), id2);
  AddSurfaceReference(id1, id2);

  protocol::DOM::Node* parent_node = FindSurfaceNode(id1, root());
  protocol::DOM::Node* child_node = FindSurfaceNode(id2, root());
  ExpectChildNodeRemoved(dom_agent()->GetRootSurfaceElement()->node_id(),
                         child_node->getNodeId());
  ExpectChildNodeInserted(parent_node->getNodeId(), 0);
}

// Verify that adding a surface reference moves a node from its parent to the
// root surface.
TEST_F(VizDevToolsTest, SurfaceReferenceRemoved) {
  viz::SurfaceId id1 = CreateFrameSinkAndSurface(kFrameSink1, 1);
  viz::SurfaceId id2 = CreateFrameSinkAndSurface(kFrameSink2, 1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);

  BuildDocument();

  RemoveSurfaceReference(id1, id2);

  protocol::DOM::Node* parent_node = FindSurfaceNode(id1, root());
  protocol::DOM::Node* child_node = FindSurfaceNode(id2, parent_node);
  ExpectChildNodeRemoved(parent_node->getNodeId(), child_node->getNodeId());
  ExpectChildNodeInserted(dom_agent()->GetRootSurfaceElement()->node_id(),
                          parent_node->getNodeId());
}

// Verify that a hierarchy of surfaces can be destroyed and cleaned up properly.
TEST_F(VizDevToolsTest, SurfaceHierarchyCleanup) {
  viz::SurfaceId parent_surface_id = CreateFrameSinkAndSurface(kFrameSink1, 1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), parent_surface_id);

  std::vector<viz::FrameSinkId> child_frame_sink_ids = {
      viz::FrameSinkId(5, 0), viz::FrameSinkId(6, 0), viz::FrameSinkId(7, 0),
      viz::FrameSinkId(8, 0), viz::FrameSinkId(9, 0),
  };

  std::vector<viz::SurfaceId> child_surface_ids;
  for (auto& frame_sink_id : child_frame_sink_ids) {
    viz::SurfaceId surface_id = CreateFrameSinkAndSurface(frame_sink_id, 1);
    AddSurfaceReference(parent_surface_id, surface_id);
    child_surface_ids.push_back(surface_id);
  }

  BuildDocument();

  RemoveSurfaceReference(surface_manager()->GetRootSurfaceId(),
                         parent_surface_id);
  DestroySurface(parent_surface_id);

  // It is safe to access |parent_node| after the surface was just destroyed
  // because updates to the frontend are not applied to |root_|.
  protocol::DOM::Node* parent_node = FindSurfaceNode(parent_surface_id, root());
  for (auto& surface_id : child_surface_ids) {
    // Each child surface should have been moved to the root surface when the
    // parent surface was removed, but it shouldn't be discarded yet.
    protocol::DOM::Node* child_node = FindSurfaceNode(surface_id, parent_node);
    ExpectChildNodeRemoved(parent_node->getNodeId(), child_node->getNodeId());
    ExpectChildNodeRemoved(dom_agent()->GetRootSurfaceElement()->node_id(),
                           child_node->getNodeId(), /*count=*/0);

    // Evict and garbage collect, this should remove the element.
    DestroySurface(surface_id);
    ExpectChildNodeRemoved(dom_agent()->GetRootSurfaceElement()->node_id(),
                           child_node->getNodeId(), /*count=*/1);
  }
}

// Verify that a surface with multiple references is only a child of its most
// recent referrer.
// TODO(sgilhuly): This test follows the current behaviour of surfaces with
// multiple references, and should be changed if support for nodes to have
// multiple parents is added.
TEST_F(VizDevToolsTest, MultipleSurfaceReferences) {
  viz::SurfaceId parent_id_1 = CreateFrameSinkAndSurface(kFrameSink1, 1);
  viz::SurfaceId parent_id_2 = CreateFrameSinkAndSurface(kFrameSink2, 1);
  viz::SurfaceId child_id = CreateFrameSinkAndSurface(kFrameSink3, 1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), parent_id_1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), parent_id_2);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), child_id);

  BuildDocument();

  protocol::DOM::Node* parent_node_1 = FindSurfaceNode(parent_id_1, root());
  protocol::DOM::Node* parent_node_2 = FindSurfaceNode(parent_id_2, root());
  protocol::DOM::Node* child_node = FindSurfaceNode(child_id, root());

  // Attach to the first parent, while still being referenced by the root
  // surface. This should move the child node.
  AddSurfaceReference(parent_id_1, child_id);
  ExpectChildNodeInserted(parent_node_1->getNodeId(), 0);
  ExpectChildNodeRemoved(dom_agent()->GetRootSurfaceElement()->node_id(),
                         child_node->getNodeId());

  // Attach to the second parent, while still being referenced by the first
  // parent. This should move the child node.
  AddSurfaceReference(parent_id_2, child_id);
  ExpectChildNodeInserted(parent_node_2->getNodeId(), 0);
  ExpectChildNodeRemoved(parent_node_1->getNodeId(), child_node->getNodeId());

  // Remove the references to the root surface, and the first parent. This
  // should do nothing.
  frontend_channel()->SetAllowNotifications(false);
  RemoveSurfaceReference(surface_manager()->GetRootSurfaceId(), child_id);
  RemoveSurfaceReference(parent_id_1, child_id);
  frontend_channel()->SetAllowNotifications(true);

  // Remove the reference to the second parent. This should move the child node
  // to the root surface, after the second parent.
  RemoveSurfaceReference(parent_id_2, child_id);
  ExpectChildNodeInserted(dom_agent()->GetRootSurfaceElement()->node_id(),
                          parent_node_2->getNodeId());
  ExpectChildNodeRemoved(parent_node_2->getNodeId(), child_node->getNodeId());
}

// Verify that a surface reference can be added if there is no node for the
// parent.
TEST_F(VizDevToolsTest, SurfaceReferenceAddedBeforeParentActivation) {
  viz::SurfaceId parent_id = CreateFrameSinkAndSurface(kFrameSink1, 1);
  viz::SurfaceId child_id = CreateFrameSinkAndSurface(kFrameSink2, 1);

  BuildDocument();

  AddSurfaceReference(parent_id, child_id);

  ExpectChildNodeInserted(dom_agent()->GetRootSurfaceElement()->node_id(), 0);
}

// Verify that a surface reference can be added if there is no node for the
// child.
TEST_F(VizDevToolsTest, SurfaceReferenceAddedBeforeChildActivation) {
  viz::SurfaceId parent_id = CreateFrameSinkAndSurface(kFrameSink1, 1);
  viz::SurfaceId child_id = CreateFrameSinkAndSurface(kFrameSink2, 1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), parent_id);

  BuildDocument();

  AddSurfaceReference(parent_id, child_id);

  protocol::DOM::Node* parent_node = FindSurfaceNode(parent_id, root());
  ExpectChildNodeInserted(parent_node->getNodeId(), 0);
}

// Verify that SurfaceElements are inserted into the tree according to the
// ordering of their SurfaceIds.
TEST_F(VizDevToolsTest, SurfaceElementOrdering) {
  viz::SurfaceId id2 = CreateFrameSinkAndSurface(kFrameSink2, 1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), id2);

  BuildDocument();

  protocol::DOM::Node* surface_node = FindSurfaceNode(id2, root());

  // Create a surface element before the existing surface.
  viz::SurfaceId id1 = CreateFrameSinkAndSurface(kFrameSink1, 1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), id1);
  ExpectChildNodeInserted(dom_agent()->GetRootSurfaceElement()->node_id(), 0);

  // Create a surface element after the existing surface.
  viz::SurfaceId id3 = CreateFrameSinkAndSurface(kFrameSink3, 1);
  AddSurfaceReference(surface_manager()->GetRootSurfaceId(), id3);
  ExpectChildNodeInserted(dom_agent()->GetRootSurfaceElement()->node_id(),
                          surface_node->getNodeId());
}

// Verify that FrameSinkElements are placed before the root SurfaceElement.
TEST_F(VizDevToolsTest, FrameSinkAndSurfaceElementOrdering) {
  RegisterFrameSinkId(kFrameSink1);

  BuildDocument();

  protocol::DOM::Node* frame_sink_node = FindFrameSinkNode(kFrameSink1, root());
  protocol::DOM::Node* root_surface_node =
      FindSurfaceNode(surface_manager()->GetRootSurfaceId(), root());

  // The frame sink should be before the root surface node.
  EXPECT_EQ(frame_sink_node, (*(root()->getChildren(nullptr)))[0].get());
  EXPECT_EQ(root_surface_node, (*(root()->getChildren(nullptr)))[1].get());

  // Create a frame sink element with a large id, it should still be inserted
  // before the root surface element.
  viz::FrameSinkId big_frame_sink(50, 50);
  RegisterFrameSinkId(big_frame_sink);
  ExpectChildNodeInserted(dom_agent()->element_root()->node_id(),
                          frame_sink_node->getNodeId());
}

}  // namespace ui_devtools
