// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_H_

#include "base/metrics/histogram_functions.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_histogram_value.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace guest_view {

// A GuestView is the templated base class for out-of-process frames in the
// chrome layer. GuestView is templated on its derived type to allow for type-
// safe access. See GuestViewBase for more information.
template <typename T>
class GuestView : public GuestViewBase {
 public:
  static T* FromGuestViewBase(GuestViewBase* guest_view_base) {
    return AsDerivedGuest(guest_view_base);
  }

  static T* FromInstanceID(int embedder_process_id, int guest_instance_id) {
    return AsDerivedGuest(
        GuestViewBase::FromInstanceID(embedder_process_id, guest_instance_id));
  }

  // Prefer using FromRenderFrameHost. See https://crbug.com/1362569.
  static T* FromWebContents(content::WebContents* contents) {
    return AsDerivedGuest(GuestViewBase::FromWebContents(contents));
  }

  static T* FromRenderFrameHost(content::RenderFrameHost* rfh) {
    return AsDerivedGuest(GuestViewBase::FromRenderFrameHost(rfh));
  }
  static T* FromRenderFrameHostId(
      const content::GlobalRenderFrameHostId& rfh_id) {
    return AsDerivedGuest(GuestViewBase::FromRenderFrameHostId(rfh_id));
  }

  static T* FromNavigationHandle(content::NavigationHandle* navigation_handle) {
    return AsDerivedGuest(
        GuestViewBase::FromNavigationHandle(navigation_handle));
  }

  static T* FromFrameTreeNodeId(content::FrameTreeNodeId frame_tree_node_id) {
    return AsDerivedGuest(
        GuestViewBase::FromFrameTreeNodeId(frame_tree_node_id));
  }

  GuestView(const GuestView&) = delete;
  GuestView& operator=(const GuestView&) = delete;

  // GuestViewBase implementation.
  const char* GetViewType() const final {
    return T::Type;
  }

 protected:
  explicit GuestView(content::RenderFrameHost* owner_rfh)
      : GuestViewBase(owner_rfh) {
    base::UmaHistogramEnumeration("GuestView.GuestViewCreated",
                                  T::HistogramValue);
  }
  ~GuestView() override = default;

  T* GetOpener() const { return AsDerivedGuest(GuestViewBase::GetOpener()); }

  void SetOpener(T* opener) { GuestViewBase::SetOpener(opener); }

 private:
  // Downcasts to a *ViewGuest if the GuestViewBase is of the derived view type.
  // Otherwise, returns nullptr.
  static T* AsDerivedGuest(GuestViewBase* guest) {
    if (!guest)
      return nullptr;

    const bool same_type = !strcmp(guest->GetViewType(), T::Type);
    if (!same_type)
      return nullptr;

    return static_cast<T*>(guest);
  }
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_H_
