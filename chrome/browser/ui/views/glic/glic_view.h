// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_

#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class Rect;
}  // namespace gfx

class Profile;

namespace glic {

class GlicView : public views::View {
 public:
  GlicView(Profile* profile, const gfx::Size& initial_size);
  GlicView(const GlicView&) = delete;
  GlicView& operator=(const GlicView&) = delete;
  ~GlicView() override;

  // Creates a menu widget that contains a `GlicView`, configured with the
  // given `initial_bounds`.
  static views::UniqueWidgetPtr CreateWidget(Profile* profile,
                                             const gfx::Rect& initial_bounds);
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_
