// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/default_browser_modal_dialog_delegate.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/browser/ui/views/web_dialogs/chrome_webui_dialog.h"
#include "chrome/browser/ui/webui/default_browser/default_browser_modal_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "default_browser_modal_dialog_delegate.h"
#include "net/base/url_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace default_browser {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kDefaultBrowserModalDialogId);

std::unique_ptr<views::Widget> Show(Profile* profile,
                                    gfx::NativeWindow parent,
                                    bool use_settings_illustration,
                                    bool can_pin_to_taskbar) {
  GURL url(chrome::kChromeUIDefaultBrowserModalURL);
  if (use_settings_illustration) {
    url = net::AppendQueryParameter(url, "illustration", "true");
  }
  if (can_pin_to_taskbar) {
    url = net::AppendQueryParameter(url, "can_pin_to_taskbar", "true");
  }
  url = net::AppendQueryParameter(url, "is_modal", "true");

  auto contents_wrapper =
      std::make_unique<WebUIContentsWrapperT<DefaultBrowserModalUI>>(
          url, profile, /*task_manager_string_id=*/0,
          /*esc_closes_ui=*/true,
          /*supports_draggable_regions=*/false);

  constexpr int kInitialWidth = 615;

  webui_dialog::WebDialogSpec spec;
  spec.min_size = gfx::Size(kInitialWidth, 100);
  spec.max_size = gfx::Size(kInitialWidth, 1000);
  spec.wait_for_explicit_show = true;
  spec.modal_type = ui::mojom::ModalType::kWindow;
  spec.element_identifier = kDefaultBrowserModalDialogId;

  return webui_dialog::ChromeWebUIDialog::Show(
      parent, std::move(contents_wrapper), spec);
}

}  // namespace default_browser
