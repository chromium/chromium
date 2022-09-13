// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_HOST_H_
#define COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_HOST_H_

#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_modal_export.h"

namespace gfx {
class Size;
}

namespace web_modal {

// Unlike browser modal dialogs, web contents modal dialogs should not be able
// to draw outside the browser window. WebContentsModalDialogHost adds a
// GetMaximumDialogSize method in order for positioning code to be able to take
// this into account.
class WEB_MODAL_EXPORT WebContentsModalDialogHost : public ModalDialogHost {
 public:
  ~WebContentsModalDialogHost() override;

  // Returns the maximum dimensions a dialog can have.
  virtual gfx::Size GetMaximumDialogSize() = 0;
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_HOST_H_
