// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/webui_page_action_view.h"

#include "base/notreached.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/views/page_action/webui_page_action_control.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace page_actions {

WebUIPageActionView::WebUIPageActionView(actions::ActionId action_id,
                                         WebUIPageActionControl& owner)
    : action_id_(action_id), owner_(owner) {}

WebUIPageActionView::~WebUIPageActionView() = default;

views::BubbleAnchor WebUIPageActionView::GetBubbleAnchor() {
  BrowserWindowInterface* browser = owner_->GetBrowser();
  if (!browser) {
    return views::BubbleAnchor();
  }

  PageActionPropertiesProvider provider;
  if (!provider.Contains(action_id_)) {
    return views::BubbleAnchor();
  }

  ui::ElementIdentifier element_id =
      provider.GetProperties(action_id_).element_identifier;
  if (!element_id) {
    return views::BubbleAnchor();
  }

  ui::TrackedElement* element =
      BrowserElements::From(browser)->GetElement(element_id);
  if (element) {
    return views::BubbleAnchor(element);
  }

  return views::BubbleAnchor();
}

std::u16string WebUIPageActionView::GetTooltipText() const {
  const PageActionModelInterface* model = owner_->GetObservedModel(action_id_);
  if (model) {
    return model->GetTooltipText();
  }
  return std::u16string();
}

std::u16string WebUIPageActionView::GetAccessibleName() const {
  const PageActionModelInterface* model = owner_->GetObservedModel(action_id_);
  if (model) {
    return model->GetAccessibleName();
  }
  return std::u16string();
}

void WebUIPageActionView::SetVisible(bool visible) {
  PageActionController* controller = owner_->GetController(action_id_);
  if (!controller) {
    return;
  }
  if (visible) {
    controller->Show(action_id_);
  } else {
    controller->Hide(action_id_);
  }
}

IconLabelBubbleView* WebUIPageActionView::GetIconLabelBubbleViewNotMigrated() {
  // It is a configuration issue if this gets called, as Chrome should never
  // start up with some page actions not migrated while running with WebUI
  // toolbar at the same time.
  NOTREACHED();
}

}  // namespace page_actions
