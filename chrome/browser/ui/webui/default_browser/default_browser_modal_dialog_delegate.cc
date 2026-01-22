// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/default_browser_modal_dialog_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace default_browser {

// static
void DefaultBrowserModalDialog::Show(Profile* profile, gfx::NativeView parent) {
  chrome::ShowWebDialog(parent, profile, new DefaultBrowserModalDialog());
}

DefaultBrowserModalDialog::DefaultBrowserModalDialog() {
  set_can_resize(false);
  set_can_maximize(false);
  set_can_minimize(false);
  set_show_close_button(false);
  set_dialog_modal_type(ui::mojom::ModalType::kWindow);
  set_dialog_content_url(GURL());
  set_dialog_size(gfx::Size(605, 590));
  set_can_close(true);
  set_show_dialog_title(false);
}

DefaultBrowserModalDialog::~DefaultBrowserModalDialog() = default;

}  // namespace default_browser
