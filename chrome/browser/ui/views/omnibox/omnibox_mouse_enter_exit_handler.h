// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_MOUSE_ENTER_EXIT_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_MOUSE_ENTER_EXIT_HANDLER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"

namespace views {
class View;
}  // namespace views

// Omnibox suggestion rows (and header rows) rely on mouse-enter and mouse-exit
// events to update their hover state (whether or not they are highlighted).
// But child Views sometimes interfere with receiving these events when the
// mouse is moved quickly, which causes rows to remain erroneously highlighted.
// This class solves that problem by bridging the child Views mouse event
// handler to a callback. This class instance must outlive all observed Views.
class OmniboxMouseEnterExitHandler : public ui::EventHandler {
 public:
  // |enter_exit_callback| is called whenever one of the observed Views get a
  // mouse-enter or mouse-exit event.
  explicit OmniboxMouseEnterExitHandler(
      base::RepeatingClosure enter_exit_callback);
  OmniboxMouseEnterExitHandler(const OmniboxMouseEnterExitHandler&) = delete;
  OmniboxMouseEnterExitHandler& operator=(const OmniboxMouseEnterExitHandler&) =
      delete;
  ~OmniboxMouseEnterExitHandler() override;

  // This instance must outlive |view|.
  void ObserveMouseEnterExitOn(views::View* view);

 private:
  void OnMouseEvent(ui::MouseEvent* event) override;

  // This is called whenever one of the |observed_views_| has a mouse-enter or
  // mouse-exit event.
  const base::RepeatingClosure enter_exit_callback_;

  // These are the Views for which we are observing mouse-enter or mouse-exit
  // events. This instance must outlive all of these Views, since these are
  // non-owning pointers, which we use in our destructor.
  std::vector<raw_ptr<views::View, VectorExperimental>> observed_views_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_MOUSE_ENTER_EXIT_HANDLER_H_
