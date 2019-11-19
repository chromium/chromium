// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIZ_DOM_AGENT_VIZ_H_
#define COMPONENTS_UI_DEVTOOLS_VIZ_DOM_AGENT_VIZ_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/ui_devtools/DOM.h"
#include "components/ui_devtools/dom_agent.h"
#include "components/viz/service/frame_sinks/frame_sink_observer.h"
#include "components/viz/service/surfaces/surface_observer.h"

namespace viz {
struct BeginFrameAck;
struct BeginFrameArgs;
class FrameSinkId;
class FrameSinkManagerImpl;
class SurfaceId;
class SurfaceInfo;
class SurfaceManager;
}  // namespace viz

namespace ui_devtools {
class FrameSinkElement;
class SurfaceElement;

class DOMAgentViz : public viz::SurfaceObserver,
                    public viz::FrameSinkObserver,
                    public DOMAgent {
 public:
  explicit DOMAgentViz(viz::FrameSinkManagerImpl* frame_sink_manager);
  ~DOMAgentViz() override;

  // viz::SurfaceObserver:
  void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;
  void OnSurfaceActivated(const viz::SurfaceId& surface_id,
                          base::Optional<base::TimeDelta> duration) override {}
  void OnSurfaceMarkedForDestruction(
      const viz::SurfaceId& surface_id) override {}
  bool OnSurfaceDamaged(const viz::SurfaceId& surface_id,
                        const viz::BeginFrameAck& ack) override;
  void OnSurfaceDestroyed(const viz::SurfaceId& surface_id) override;
  void OnSurfaceDamageExpected(const viz::SurfaceId& surface_id,
                               const viz::BeginFrameArgs& args) override {}
  void OnAddedSurfaceReference(const viz::SurfaceId& parent_id,
                               const viz::SurfaceId& child_id) override;
  void OnRemovedSurfaceReference(const viz::SurfaceId& parent_id,
                                 const viz::SurfaceId& child_id) override;

  // viz::FrameSinkObserver:
  void OnRegisteredFrameSinkId(const viz::FrameSinkId& frame_sink_id) override;
  void OnInvalidatedFrameSinkId(const viz::FrameSinkId& frame_sink_id) override;
  void OnCreatedCompositorFrameSink(const viz::FrameSinkId& frame_sink_id,
                                    bool is_root) override;
  void OnDestroyedCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id) override;
  void OnRegisteredFrameSinkHierarchy(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& child_frame_sink_id) override;
  void OnUnregisteredFrameSinkHierarchy(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& child_frame_sink_id) override;
  void OnFrameSinkDidBeginFrame(const viz::FrameSinkId& frame_sink_id,
                                const viz::BeginFrameArgs& args) override {}
  void OnFrameSinkDidFinishFrame(const viz::FrameSinkId& frame_sink_id,
                                 const viz::BeginFrameArgs& args) override {}

  // DOM::Backend:
  protocol::Response enable() override;
  protocol::Response disable() override;

  SurfaceElement* GetRootSurfaceElement();

 private:
  std::unique_ptr<protocol::DOM::Node> BuildTreeForFrameSink(
      UIElement* parent_element,
      const viz::FrameSinkId& parent_id);

  std::unique_ptr<protocol::DOM::Node> BuildTreeForSurface(
      UIElement* parent_element,
      const viz::SurfaceId& parent_id);

  // DOMAgent:
  std::vector<UIElement*> CreateChildrenForRoot() override;
  std::unique_ptr<protocol::DOM::Node> BuildTreeForUIElement(
      UIElement* ui_element) override;

  // Every time the frontend disconnects we don't destroy DOMAgent so once we
  // establish the connection again we need to clear the FrameSinkId sets
  // because they may carry obsolete data. Then we initialize these with alive
  // FrameSinkIds. Clears the sets of FrameSinkIds that correspond to created
  // FrameSinks, registered FrameSinkIds and those that have corresponding
  // FrameSinkElements created.
  void Clear();

  // Destroy |element| and attach all its children to the root_element().
  void DestroyElementAndRemoveSubtree(UIElement* element);

  // Removes an element from either |frame_sink_elements_| or
  // |surface_elements_|.
  void DestroyElement(UIElement* element);

  // Constructs a new FrameSinkElement with some default arguments, adds it to
  // |frame_sink_elements_|, and returns its pointer.
  FrameSinkElement* CreateFrameSinkElement(
      const viz::FrameSinkId& frame_sink_id,
      UIElement* parent,
      bool is_root,
      bool is_client_connected);

  // Constructs a new SurfaceElement with some default arguments, adds it to
  // |surface_elements_|, and returns its pointer.
  SurfaceElement* CreateSurfaceElement(const viz::SurfaceId& surface_id,
                                       UIElement* parent);

  // This is used to track created FrameSinkElements in a FrameSink tree. Every
  // time we register/invalidate a FrameSinkId, create/destroy a FrameSink,
  // register/unregister hierarchy we change this set, because these actions
  // involve deleting and adding elements.
  base::flat_map<viz::FrameSinkId, std::unique_ptr<FrameSinkElement>>
      frame_sink_elements_;

  // This is used to track created SurfaceElements and will be used for updates
  // in a Surface tree.
  base::flat_map<viz::SurfaceId, std::unique_ptr<SurfaceElement>>
      surface_elements_;

  viz::FrameSinkManagerImpl* frame_sink_manager_;
  viz::SurfaceManager* surface_manager_;

  DISALLOW_COPY_AND_ASSIGN(DOMAgentViz);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIZ_DOM_AGENT_VIZ_H_
