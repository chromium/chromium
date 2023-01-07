// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_CONTAINER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "ui/gfx/geometry/size.h"

namespace views {
class DialogDelegateView;
class View;
}

// Creates a new native dialog of the given |size| containing |view| with a
// close button and draggable titlebar.
views::DialogDelegateView* CreateDialogContainerForView(
    std::unique_ptr<views::View> view,
    const gfx::Size& size,
    base::OnceClosure close_callback);

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_CONTAINER_H_
