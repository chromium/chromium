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

}  // namespace web_modal
