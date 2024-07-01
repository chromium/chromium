// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMPOSE_CHROME_COMPOSE_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_COMPOSE_CHROME_COMPOSE_DIALOG_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/compose/compose_dialog_view.h"
#include "chrome/browser/ui/webui/compose/compose_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_dialog_controller.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class RectF;
}

// Controls how Compose dialogs are shown and hidden, and animations related to
// both actions.
class ChromeComposeDialogController : public compose::ComposeDialogController,
                                      views::WidgetObserver {
 public:
  using ComposeClient = compose::ComposeClient;

  ChromeComposeDialogController(content::WebContents* contents,
                                ComposeClient::FieldIdentifier field_ids);
  ~ChromeComposeDialogController() override;

  // Create and show the dialog view.
  void ShowComposeDialog(views::View* anchor_view,
                         const gfx::RectF& element_bounds_in_screen);

  // Returns the currently shown compose dialog. Returns nullptr if the dialog
  // is not currently shown.
  WebUIContentsWrapperT<ComposeUntrustedUI>* GetBubbleWrapper() const;

  // Shows the current dialog view, if there is one.
  // TODO(b/328730979): `focus_lost_callback` is called after some delay after
  // the compose dialog loses focus. The delay is configurable via
  // ComposeConfig. The purpose of the delay is so that `focus_lost_callback` is
  // called after all focus-related events have been processed.
  void ShowUI(base::OnceClosure focus_lost_callback) override;

  void Close() override;

  bool IsDialogShowing() override;

  // views::WidgetObserver implementation.
  // The destroying event occurs immediately before the widget is destroyed.
  void OnWidgetDestroying(views::Widget* widget) override;

  // Returns an identifier for the field that this dialog is acting upon.  This
  // can be used to connect to the correct session.
  const ComposeClient::FieldIdentifier& GetFieldIds() override;

 private:
  friend class ChromeComposeDialogControllerTest;

  // Called after the compose dialog loses focus.
  void OnAfterWidgetDestroyed();

  base::WeakPtr<ComposeDialogView> bubble_;
  base::WeakPtr<content::WebContents> web_contents_;

  // Identifies the field that this dialog is acting upon.
  ComposeClient::FieldIdentifier field_ids_;

  // Called when focus is lost on the compose dialog. This is not called in any
  // action that deletes a compose session, such as clicking the close button.
  base::OnceClosure focus_lost_callback_;

  // Observer for the compose widget.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::WeakPtrFactory<ChromeComposeDialogController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMPOSE_CHROME_COMPOSE_DIALOG_CONTROLLER_H_
