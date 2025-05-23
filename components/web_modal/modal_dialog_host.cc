// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/modal_dialog_host.h"

namespace web_modal {

ModalDialogHostObserver::~ModalDialogHostObserver() {
}

ModalDialogHost::~ModalDialogHost() {
}

bool ModalDialogHost::ShouldActivateDialog() const {
  return true;
}

bool ModalDialogHost::ShouldDialogBoundsConstrainedByHost() {
  // Please consult with //constrained_window OWNERS if you intend to release
  // the bounds constraint for your WebContents container (i.e. returning
  // false from this function).
  return true;
}

}  // namespace web_modal
