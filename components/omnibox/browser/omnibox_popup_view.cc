// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_view.h"

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "components/omnibox/browser/omnibox_controller.h"

OmniboxPopupView::OmniboxPopupView(OmniboxController* controller)
    : controller_(controller) {}

OmniboxPopupView::~OmniboxPopupView() = default;

OmniboxEditModel* OmniboxPopupView::model() {
  return const_cast<OmniboxEditModel*>(
      const_cast<const OmniboxPopupView*>(this)->model());
}

const OmniboxEditModel* OmniboxPopupView::model() const {
  return controller_->edit_model();
}

OmniboxController* OmniboxPopupView::controller() {
  return const_cast<OmniboxController*>(
      const_cast<const OmniboxPopupView*>(this)->controller());
}

const OmniboxController* OmniboxPopupView::controller() const {
  return controller_;
}

base::CallbackListSubscription OmniboxPopupView::AddOpenListener(
    base::RepeatingClosure callback) {
  return on_popup_callbacks_.Add(std::move(callback));
}

void OmniboxPopupView::NotifyOpenListeners() {
  on_popup_callbacks_.Notify();
}
