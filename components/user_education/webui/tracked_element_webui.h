// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_WEBUI_TRACKED_ELEMENT_WEBUI_H_
#define COMPONENTS_USER_EDUCATION_WEBUI_TRACKED_ELEMENT_WEBUI_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/rect_f.h"

namespace user_education {

class HelpBubbleHandlerBase;

class TrackedElementWebUI : public ui::TrackedElement {
 public:
  TrackedElementWebUI(HelpBubbleHandlerBase* handler,
                      ui::ElementIdentifier identifier,
                      ui::ElementContext context);
  ~TrackedElementWebUI() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  HelpBubbleHandlerBase* handler() const { return handler_; }

  // ui::TrackedElement:
  gfx::Rect GetScreenBounds() const override;

 private:
  friend class HelpBubbleHandlerBase;

  void SetVisible(bool visible, gfx::RectF bounds = gfx::RectF());
  void Activate();
  void CustomEvent(ui::CustomElementEventType event_type);
  bool visible() const { return visible_; }

  const raw_ptr<HelpBubbleHandlerBase> handler_;
  bool visible_ = false;
  gfx::RectF last_known_bounds_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_WEBUI_TRACKED_ELEMENT_WEBUI_H_