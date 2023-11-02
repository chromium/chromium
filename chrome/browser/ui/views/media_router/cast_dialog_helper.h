// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_HELPER_H_

#include <memory>

#include "ui/views/view.h"

namespace media_router {

// Icon sizes in DIP.
constexpr int kPrimaryIconSize = 20;
constexpr auto kPrimaryIconBorder = gfx::Insets(6);

// Creates a view containing a throbber. The throbber has a border around it so
// that the view's size is the same with the primary icon with its border.
std::unique_ptr<views::View> CreateThrobber();

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_HELPER_H_
