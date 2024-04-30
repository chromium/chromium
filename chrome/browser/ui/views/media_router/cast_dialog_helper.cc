// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"

#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/fill_layout.h"

namespace media_router {

std::unique_ptr<views::View> CreateThrobber() {
  views::Throbber* throbber = new views::Throbber();
  throbber->Start();
  auto throbber_container = std::make_unique<views::View>();
  throbber_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  // The throbber is smaller than other icons, so the difference must be added
  // to the border to make their overall sizes match.
  const int extra_borders =
      kPrimaryIconSize - throbber->CalculatePreferredSize({}).height();
  throbber_container->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(extra_borders / 2) + kPrimaryIconBorder));
  throbber_container->AddChildView(throbber);
  return throbber_container;
}

}  // namespace media_router
