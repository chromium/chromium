// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMPOSE_CHROME_COMPOSE_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_COMPOSE_CHROME_COMPOSE_DIALOG_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/compose/compose_dialog_view.h"
#include "chrome/browser/ui/webui/compose/compose_ui.h"
#include "components/compose/core/browser/compose_dialog_controller.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace gfx {
class RectF;
}

// Controls how Compose dialogs are shown and hidden, and animations related to
// both actions.
class ChromeComposeDialogController : public compose::ComposeDialogController {
 public:
  explicit ChromeComposeDialogController(content::WebContents* contents);
  ~ChromeComposeDialogController() override;

  // Create and show the dialog view.
  void ShowComposeDialog(views::View* anchor_view,
                         const gfx::RectF& element_bounds_in_screen);

  // Returns the currently shown compose dialog. Returns nullptr if the dialog
  // is not currently shown.
  BubbleContentsWrapperT<ComposeUI>* GetBubbleWrapper() const;

  void Close() override;

 private:
  friend class ChromeComposeDialogControllerTest;

  std::unique_ptr<BubbleContentsWrapperT<ComposeUI>> bubble_wrapper_;
  base::WeakPtr<content::WebContents> web_contents_;

  base::WeakPtrFactory<ChromeComposeDialogController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMPOSE_CHROME_COMPOSE_DIALOG_CONTROLLER_H_
