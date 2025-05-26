// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_CONTENTS_BROWSER_GUEST_CONTENTS_HANDLE_H_
#define COMPONENTS_GUEST_CONTENTS_BROWSER_GUEST_CONTENTS_HANDLE_H_

#include "base/sequence_checker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace guest_contents {

using GuestId = int;

// A handle for a guest WebContents, uniquely identified by a GuestId. It
// provides APIs for attaching and detaching the guest WebContents from the
// outer WebContents. It has the same lifetime as the guest WebContents.
class GuestContentsHandle
    : public content::WebContentsUserData<GuestContentsHandle>,
      public content::WebContentsObserver {
 public:
  ~GuestContentsHandle() override;

  // Creates a handle for the given `web_contents`.
  static GuestContentsHandle* CreateForWebContents(
      content::WebContents* web_contents);

  static GuestContentsHandle* FromID(GuestId id);

  // Returns the unique ID of this guest. This ID is automatically assigned
  // when the GuestContentsHandle is created.
  GuestId id() { return id_; }

  void AttachToOuterWebContents(
      content::RenderFrameHost* outer_delegate_frame);

  void DetachFromOuterWebContents();

 private:
  friend content::WebContentsUserData<GuestContentsHandle>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit GuestContentsHandle(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  GuestId GetNextId();

  GuestId id_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace guest_contents

#endif  // COMPONENTS_GUEST_CONTENTS_BROWSER_GUEST_CONTENTS_HANDLE_H_
