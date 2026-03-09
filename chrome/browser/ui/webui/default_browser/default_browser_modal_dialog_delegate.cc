// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/default_browser_modal_dialog_delegate.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/dialogs/browser_dialogs.h"
#include "chrome/common/webui_url_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
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
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace default_browser {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DefaultBrowserModalDialog,
                                      kDefaultBrowserModalDialogId);

// static
views::Widget* DefaultBrowserModalDialog::Show(Profile* profile,
                                               gfx::NativeWindow parent,
                                               bool use_settings_illustration,
                                               bool can_pin_to_taskbar) {
  auto delegate = std::make_unique<DefaultBrowserModalDialog>(
      profile, use_settings_illustration, can_pin_to_taskbar);
  return constrained_window::CreateBrowserModalDialogViews(std::move(delegate),
                                                           parent);
}

DefaultBrowserModalDialog::DefaultBrowserModalDialog(
    Profile* profile,
    bool use_settings_illustration,
    bool can_pin_to_taskbar) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetModalType(ui::mojom::ModalType::kWindow);
  SetShowCloseButton(false);

  auto web_view = std::make_unique<views::WebView>(profile);
  web_view_ = web_view.get();

  GURL url(chrome::kChromeUIDefaultBrowserModalURL);
  if (use_settings_illustration) {
    url = net::AppendQueryParameter(url, "illustration", "true");
  }
  if (can_pin_to_taskbar) {
    url = net::AppendQueryParameter(url, "can_pin_to_taskbar", "true");
  }
  url = net::AppendQueryParameter(url, "is_modal", "true");
  web_view_->LoadInitialURL(url);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const int kInitialWidth = 605;
#else
  const int kInitialWidth = 615;
#endif
  const int kInitialHeight = 630;
  web_view_->SetPreferredSize(gfx::Size(kInitialWidth, kInitialHeight));

  web_view_->SetProperty(views::kElementIdentifierKey,
                         kDefaultBrowserModalDialogId);

  web_view_observation_.Observe(web_view_);

  SetContentsView(std::move(web_view));
}

DefaultBrowserModalDialog::~DefaultBrowserModalDialog() = default;

void DefaultBrowserModalDialog::ResizeNativeViewHeight(int height) {
  CHECK(web_view_);

  gfx::Size size = web_view_->GetPreferredSize();
  size.set_height(height);
  web_view_->SetPreferredSize(size);
  if (GetWidget()) {
    GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
  }
}

void DefaultBrowserModalDialog::OnViewAddedToWidget(
    views::View* observed_view) {
  CHECK_EQ(observed_view, web_view_);

  if (!web_view_->GetWebContents() ||
      !web_view_->GetWebContents()->GetWebUI()) {
    return;
  }

  web_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(GetCornerRadius()));
}

void DefaultBrowserModalDialog::OnViewIsDeleting(views::View* observed_view) {
  CHECK_EQ(observed_view, web_view_);

  web_view_observation_.Reset();
  web_view_ = nullptr;

  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

}  // namespace default_browser
