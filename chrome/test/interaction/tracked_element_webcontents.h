// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_INTERACTION_TRACKED_ELEMENT_WEBCONTENTS_H_
#define CHROME_TEST_INTERACTION_TRACKED_ELEMENT_WEBCONTENTS_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"

class WebContentsInteractionTestUtil;

// Represents a loaded web page. Created and shown by
// WebContentsInteractionTestUtil when the WebContents it is watching fully
// loads a page and then hidden and destroyed when the page unloads, navigates
// away, or is closed.
class TrackedElementWebContents : public ui::TrackedElement {
 public:
  TrackedElementWebContents(ui::ElementIdentifier identifier,
                            ui::ElementContext context,
                            WebContentsInteractionTestUtil* owner);
  ~TrackedElementWebContents() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // Event generated when the WebContents receives its first non-empty paint.
  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kFirstNonEmptyPaint);

  // TrackedElement:
  gfx::Rect GetScreenBounds() const override;
  std::string ToString() const override;

  WebContentsInteractionTestUtil* owner() { return owner_; }
  const WebContentsInteractionTestUtil* owner() const { return owner_; }

 private:
  friend WebContentsInteractionTestUtil;

  void Init();

  const raw_ptr<WebContentsInteractionTestUtil> owner_;
};

#endif  // CHROME_TEST_INTERACTION_TRACKED_ELEMENT_WEBCONTENTS_H_
