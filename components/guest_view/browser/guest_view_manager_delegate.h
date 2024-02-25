// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MANAGER_DELEGATE_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MANAGER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/values.h"

namespace content {
class WebContents;
}  // namespace content

namespace guest_view {

class GuestViewBase;
class GuestViewManager;

// A GuestViewManagerDelegate interface allows GuestViewManager to delegate
// responsibilities to other modules in Chromium. Different builds of Chromium
// may use different GuestViewManagerDelegate implementations. For example,
// mobile builds of Chromium do not include an extensions module and so
// permission checks would be different, and IsOwnedByExtension would always
// return false.
class GuestViewManagerDelegate {
 public:
  GuestViewManagerDelegate();
  virtual ~GuestViewManagerDelegate();

  // Invoked after |guest_web_contents| is added.
  virtual void OnGuestAdded(content::WebContents* guest_web_contents) const {}

  // Dispatches the event with |name| with the provided |args| to the embedder
  // of the given |guest| with |instance_id| for routing.
  virtual void DispatchEvent(const std::string& event_name,
                             base::Value::Dict args,
                             GuestViewBase* guest,
                             int instance_id) {}

  // Indicates whether the |guest| can be used within the context of where it
  // was created.
  virtual bool IsGuestAvailableToContext(const GuestViewBase* guest) const;

  // Indicates whether the |guest| is owned by an extension or Chrome App.
  virtual bool IsOwnedByExtension(const GuestViewBase* guest);

  // Indicates whether the |guest| is owned by a Controlled Frame embedder.
  virtual bool IsOwnedByControlledFrameEmbedder(const GuestViewBase* guest);

  // Registers additional GuestView types the delegator (GuestViewManger) can
  // create.
  virtual void RegisterAdditionalGuestViewTypes(GuestViewManager* manager) {}
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MANAGER_DELEGATE_H_
