// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_AX_TREE_SOURCE_FLUTTER_H_
#define CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_AX_TREE_SOURCE_FLUTTER_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "chromecast/browser/accessibility/proto/cast_server_accessibility.pb.h"
#include "chromecast/browser/cast_web_contents.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_source.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class AutomationEventRouterInterface;
}  // namespace extensions

namespace ui {
struct AXEvent;
}  // namespace ui

namespace chromecast {
namespace accessibility {

class FlutterSemanticsNode;

// This class translates accessibility trees found in the gallium accessibility
// OnAccessibilityEventRequest proto into a tree update Chrome's accessibility
// API can work with.
class AXTreeSourceFlutter : public ui::AXTreeSource<FlutterSemanticsNode*>,
                            public CastWebContentsObserver,
                            public ui::AXActionHandler {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnAction(const ui::AXActionData& data) = 0;
    virtual void OnVirtualKeyboardBoundsChange(const gfx::Rect& bounds) = 0;
  };

  AXTreeSourceFlutter(
      Delegate* delegate,
      content::BrowserContext* browser_context,
      extensions::AutomationEventRouterInterface* event_router = nullptr);
  AXTreeSourceFlutter(const AXTreeSourceFlutter&) = delete;
  ~AXTreeSourceFlutter() override;
  AXTreeSourceFlutter& operator=(const AXTreeSourceFlutter&) = delete;

  // AXTreeSource implementation.
  bool GetTreeData(ui::AXTreeData* data) const override;

  // AXTreeSource implementation used by FlutterAccessibilityInfoData
  // subclasses.
  FlutterSemanticsNode* GetRoot() const override;
  FlutterSemanticsNode* GetFromId(int32_t id) const override;
  void SerializeNode(FlutterSemanticsNode* node,
                     ui::AXNodeData* out_data) const override;
  FlutterSemanticsNode* GetParent(FlutterSemanticsNode* node) const override;

  // Notifies automation of an accessibility event.
  void NotifyAccessibilityEvent(
      const ::gallium::castos::OnAccessibilityEventRequest* event_data);

  // Notifies automation of a result to an action.
  void NotifyActionResult(const ui::AXActionData& data, bool result);

  // Attaches tree to an aura window and gives it system focus.
  void Focus(aura::Window* window);

  // Gets the window id of this tree.
  int32_t window_id() const { return window_id_; }

  void UpdateTree();

  // CastWebContentsObserver
  void PageStopped(PageState page_state, int error_code) override;

  void SetAccessibilityEnabled(bool value);

 private:
  class AXTreeWebContentsObserver : public content::WebContentsObserver {
   public:
    AXTreeWebContentsObserver(
        content::WebContents* web_contents,
        chromecast::accessibility::AXTreeSourceFlutter* ax_tree_source);

    AXTreeWebContentsObserver(const AXTreeWebContentsObserver&) = delete;
    AXTreeWebContentsObserver& operator=(const AXTreeWebContentsObserver&) =
        delete;

    void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                                content::RenderFrameHost* new_host) override;

    void AXTreeIDForMainFrameHasChanged() override;

   private:
    chromecast::accessibility::AXTreeSourceFlutter* ax_tree_source_;
  };

  using AXTreeFlutterSerializer = ui::AXTreeSerializer<FlutterSemanticsNode*>;

  friend class AXTreeSourceFlutterTest;

  // AXTreeSource overrides.
  int32_t GetId(FlutterSemanticsNode* node) const override;
  void GetChildren(
      FlutterSemanticsNode* node,
      std::vector<FlutterSemanticsNode*>* out_children) const override;
  bool IsValid(FlutterSemanticsNode* node) const override;
  bool IsIgnored(FlutterSemanticsNode* node) const override;
  bool IsEqual(FlutterSemanticsNode* node1,
               FlutterSemanticsNode* node2) const override;
  FlutterSemanticsNode* GetNull() const override;

  // Computes the smallest rect that encloses all of the descendants of |node|.
  gfx::Rect ComputeEnclosingBounds(FlutterSemanticsNode* node) const;

  // Helper to recursively compute bounds for |node|. Returns true if non-empty
  // bounds were encountered.
  void ComputeEnclosingBoundsInternal(FlutterSemanticsNode* node,
                                      gfx::Rect* computed_bounds) const;

  // AXHostDelegate implementation.
  void PerformAction(const ui::AXActionData& data) override;

  // Resets tree state.
  void Reset();

  // Detects live region changes and generates events for them.
  void HandleLiveRegions(std::vector<ui::AXEvent>* events);

  // Detects added or deleted routes that trigger TTS from edge
  // transitions (i.e. alert dialogs).
  void HandleRoutes(std::vector<ui::AXEvent>* events);

  // Detects rapidly changing nodes and use native TTS instead.
  void HandleNativeTTS();

  // Handle the virtual keyboard nodes and calculate the bounds of it.
  void HandleVirtualKeyboardNodes();

  // Depth first search for a node under 'parent' with names route flag.
  FlutterSemanticsNode* FindRoutesNode(FlutterSemanticsNode* parent);

  // Find the first focusable node from the root.  If none found, return
  // the root node id.
  int32_t FindFirstFocusableNodeId();

  // Submit text to TTS engine.
  void SubmitTTS(const std::string& text);

  std::unique_ptr<AXTreeFlutterSerializer> current_tree_serializer_;
  int32_t root_id_;
  int32_t window_id_;
  int32_t focused_id_;

  // A delegate that handles accessibility actions on behalf of this tree. The
  // delegate is valid during the lifetime of this tree.
  Delegate* delegate_;
  content::BrowserContext* const browser_context_;
  extensions::AutomationEventRouterInterface* const event_router_;

  // Maps a node id to its tree data.
  base::flat_map<int32_t /* node_id */, std::unique_ptr<FlutterSemanticsNode>>
      tree_map_;

  // Maps a node id to its parent.
  base::flat_map<int32_t /* node_id */, int32_t /* parent_node_id */>
      parent_map_;

  // Mapping from ArcAccessibilityInfoData ID to its cached computed bounds.
  // This simplifies bounds calculations.
  base::flat_map<int32_t, gfx::Rect> cached_computed_bounds_;

  // Cache from node id to computed name for live region.
  std::map<int32_t, std::string> live_region_name_cache_;

  // Cache for nodes with scopes route flags.
  std::vector<int32_t> scopes_route_cache_;

  // Cache form node id to tts string for native tts components.
  std::map<int32_t, std::string> native_tts_name_cache_;

  std::vector<int32_t> reparented_children_;
  std::vector<std::string> child_trees_;

  // Maps web contents id to the web contents observer
  base::flat_map<int32_t, std::unique_ptr<AXTreeWebContentsObserver>>
      child_tree_observers_;

  // Observed CastWebContents for this tree node.
  CastWebContents* cast_web_contents_;

  // Copy of most recent tree data
  gallium::castos::OnAccessibilityEventRequest last_event_data_;

  bool accessibility_enabled_ = false;

  // The bounds of virtual keyboard.
  gfx::Rect keyboard_bounds_;
};

}  // namespace accessibility
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ACCESSIBILITY_FLUTTER_AX_TREE_SOURCE_FLUTTER_H_
