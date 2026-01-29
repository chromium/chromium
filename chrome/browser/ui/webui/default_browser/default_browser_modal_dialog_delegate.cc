// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/default_browser_modal_dialog_delegate.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/url_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace default_browser {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DefaultBrowserModalDialog,
                                      kDefaultBrowserModalDialogId);

// static
void DefaultBrowserModalDialog::Show(Profile* profile,
                                     gfx::NativeView parent,
                                     bool use_settings_illustration) {
  chrome::ShowWebDialog(
      parent, profile,
      new DefaultBrowserModalDialog(use_settings_illustration));
}

DefaultBrowserModalDialog::DefaultBrowserModalDialog(
    bool use_settings_illustration) {
  set_can_resize(false);
  set_can_maximize(false);
  set_can_minimize(false);
  set_show_close_button(false);
  set_dialog_modal_type(ui::mojom::ModalType::kWindow);

  GURL url(chrome::kChromeUIDefaultBrowserModalURL);
  if (use_settings_illustration) {
    url = net::AppendQueryParameter(url, "illustration", "true");
  }
  set_dialog_content_url(url);

  const int height = use_settings_illustration ? 590 : 512;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  set_dialog_size(gfx::Size(605, height));
#else
  set_dialog_size(gfx::Size(615, height));
#endif
  set_can_close(true);
  set_show_dialog_title(false);
  set_web_view_element_id(kDefaultBrowserModalDialogId);
}

DefaultBrowserModalDialog::~DefaultBrowserModalDialog() = default;

}  // namespace default_browser
