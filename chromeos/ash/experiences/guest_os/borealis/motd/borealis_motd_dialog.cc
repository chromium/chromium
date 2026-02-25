// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_dialog.h"

#include <memory>

#include "ash/constants/url_constants.h"
#include "ash/constants/webui_url_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "chromeos/ash/experiences/guest_os/borealis/borealis_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"
#include "url/gurl.h"

namespace borealis {

namespace {

class MOTDWebContentsHandler
    : public ui::WebDialogWebContentsDelegate::WebContentsHandler {
 public:
  MOTDWebContentsHandler() = default;
  ~MOTDWebContentsHandler() override = default;

  content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    return nullptr;  //  Block navigation
  }

  void AddNewContents(content::BrowserContext* context,
                      content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture) override {
    // Empty - blocks popups
  }

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override {
    // Empty - blocks file access
  }
};

}  // namespace

BorealisMOTDDialog::BorealisMOTDDialog(content::BrowserContext* context,
                                       OnMotdClosedCallback callback)
    : close_callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  set_allow_default_context_menu(false);
  set_can_close(true);
  set_can_resize(false);
  set_dialog_content_url(GURL(ash::kChromeUIBorealisMOTDURL));
  set_dialog_frame_kind(ui::WebDialogDelegate::FrameKind::kDialog);
  set_dialog_modal_type(ui::mojom::ModalType::kSystem);
  set_dialog_size(
      gfx::Size(views::LayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH),
                views::LayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_MODAL_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT)));
  set_show_close_button(true);
  set_show_dialog_title(false);

  views::Widget::InitParams widget_params{
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET};
  widget_params.z_order = ui::ZOrderLevel::kFloatingWindow;

  // The delegate is owned by the NativeWidgetAura that will manage its deletion
  // when the native widget closes and deletes itself.
  widget_params.delegate = new views::WebDialogView(
      context, this, std::make_unique<MOTDWebContentsHandler>());
  widget_params.parent =
      ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                               ash::kShellWindowId_SystemModalContainer);

  // The widget will also be owned by the NativeWidgetAura and deleted when
  // the native window closes.
  //
  // Event flow of dialog destruction is:
  // - User closes the platform window
  // - aura::Window receives a close event, NativeWidgetAura::Close() is called
  // - because of the NATIVE_WIDGET_OWNS_WIDGET ownership, the native widget
  //   deletes the widget
  // - Widget calls OnDialogWillClose() for its delegate
  // - WebDialogView::OnDialogWillClose is called
  // - This causes BorealisMOTDDialog::OnDialogClosed() to be called
  // - BorealisMOTDDialog deletes itself
  views::Widget* widget = new views::Widget();
  widget->Init(std::move(widget_params));

  widget->Show();
}

BorealisMOTDDialog::~BorealisMOTDDialog() = default;

// static
void BorealisMOTDDialog::Show(content::BrowserContext* context,
                              OnMotdClosedCallback callback) {
  // BorealisMOTDDialog is self-deleting via OnDialogClosed().
  new BorealisMOTDDialog(context, std::move(callback));
}

// static
void BorealisMOTDDialog::MaybeShow(content::BrowserContext* context,
                                   OnMotdClosedCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kShowBorealisMotd)) {
    std::move(callback).Run(UserMotdAction::kDismiss);
    return;
  }

  BorealisMOTDDialog::Show(context, std::move(callback));
}

void BorealisMOTDDialog::OnDialogClosed(const std::string& json_retval) {
  // If the user performed an action to close the dialog such as
  // clicking X button, the default kDismiss action is kept
  UserMotdAction user_action = UserMotdAction::kDismiss;
  if (!json_retval.empty()) {
    user_action = GetUserActionFromString(json_retval);
  }

  std::move(close_callback_).Run(user_action);
  delete this;
}

}  // namespace borealis
