// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/constrained_window/native_web_contents_modal_dialog_manager_views.h"
#include "components/web_modal/single_web_contents_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"

namespace constrained_window {

void ShowModalDialog(gfx::NativeWindow dialog,
                     content::WebContents* web_contents) {
  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  DCHECK(manager);
  std::unique_ptr<web_modal::SingleWebContentsDialogManager> dialog_manager(
      new constrained_window::NativeWebContentsModalDialogManagerViews(
          dialog, manager));
  manager->ShowDialogWithManager(dialog, std::move(dialog_manager));
}

}  // namespace constrained_window
