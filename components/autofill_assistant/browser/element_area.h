// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_AREA_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_AREA_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/selector.h"

namespace autofill_assistant {
class ScriptExecutorDelegate;

// A helper that keeps track of the area on the screen that correspond to an
// changeable set of elements.
class ElementArea {
 public:
  // |delegate| and |settings| must remain valid for the lifetime of this
  // instance.
  explicit ElementArea(ScriptExecutorDelegate* delegate);
  ~ElementArea();

  // Clears the area. Stops scheduled updates.
  void Clear();

  // Updates the area and keep checking for the element position and reporting
  // it until the area is cleared.
  //
  // The area is updated asynchronously, so Contains will not work right away.
  void SetFromProto(const ElementAreaProto& proto);

  // Forces an out-of-schedule update of the viewport and positions right away.
  //
  // This method is never strictly necessary. It is useful to call it when
  // there's a reason to think the positions might have changed, to speed up
  // updates.
  //
  // Does nothing if the area is empty.
  void Update();

  // Defines a callback that'll be run every time the set of element coordinates
  // changes.
  //
  // The argument reports the areas that corresponds to currently known
  // elements, which might be empty.
  void SetOnUpdate(
      base::RepeatingCallback<void(const RectF& visual_viewport,
                                   const std::vector<RectF>& touchable_area,
                                   const std::vector<RectF>& restricted_area)>
          cb) {
    on_update_ = cb;
  }

  // Gets the position on the screen of all the rectangles that correspond to
  // the configured area.
  //
  // Each element in the vector corresponds to a rectangle, which might or might
  // not be empty.
  //
  // Note that the vector is not cleared before rectangles are added.
  void GetTouchableRectangles(std::vector<RectF>* area);
  void GetRestrictedRectangles(std::vector<RectF>* area);

  // Gets the coordinates of the visual viewport, in CSS pixels relative to the
  // layout viewport. Empty if the size of the visual viewport is not known.
  void GetVisualViewport(RectF* visual_viewport) {
    *visual_viewport = visual_viewport_;
  }

 private:
  // A rectangle that corresponds to the area of the visual viewport covered by
  // an element. Coordinates are values between 0 and 1, relative to the size of
  // the visible viewport.
  struct ElementPosition {
    // Selector. Might be empty.
    Selector selector;

    // Rectangle that corresponds to the selector. Might be empty.
    RectF rect;

    // If true, we're waiting for an updated rectangle for this
    // position.
    bool pending_update = false;

    ElementPosition();
    ElementPosition(const ElementPosition& orig);
    ~ElementPosition();
  };

  // A rectangular area, defined by its elements.
  struct Rectangle {
    std::vector<ElementPosition> positions;
    bool full_width = false;
    bool restricted = false;

    Rectangle();
    Rectangle(const Rectangle& orig);
    ~Rectangle();

    // A rectangle is pending if at least one ElementPosition is pending.
    bool IsPending() const;

    // Fills the given rectangle from the current state, if possible.
    void FillRect(RectF* rect, const RectF& visual_viewport) const;
  };

  void AddRectangles(const ::google::protobuf::RepeatedPtrField<
                         ElementAreaProto::Rectangle>& rectangles_proto,
                     bool restricted);
  void OnGetElementPosition(const Selector& selector,
                            bool found,
                            const RectF& rect);
  void OnGetVisualViewport(bool success, const RectF& rect);
  void ReportUpdate();

  ScriptExecutorDelegate* const delegate_;
  std::vector<Rectangle> rectangles_;

  // If true, update for the visual viewport position is currently scheduled.
  bool visual_viewport_pending_update_ = false;

  // Visual viewport coordinates, in CSS pixels, relative to the layout
  // viewport.
  RectF visual_viewport_;

  // While running, regularly calls Update().
  base::RepeatingTimer timer_;

  base::RepeatingCallback<void(const RectF& visual_viewport,
                               const std::vector<RectF>& touchable_area,
                               const std::vector<RectF>& restricted_area)>
      on_update_;

  base::WeakPtrFactory<ElementArea> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ElementArea);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ELEMENT_AREA_H_
