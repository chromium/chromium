// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/product_specifications_disclosure_dialog.h"

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/chrome_web_dialog_view.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

constexpr gfx::Size kDialogSize{470, 350};

void UpdateDialogPosition(views::Widget* widget,
                          content::WebContents* web_contents) {
  auto* dialog_host =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
          ->delegate()
          ->GetWebContentsModalDialogHost();
  views::Widget* host_widget =
      views::Widget::GetWidgetForNativeView(dialog_host->GetHostView());
  auto size = widget->GetRootView()->GetPreferredSize();
  if (!host_widget) {
    widget->SetSize(size);
    return;
  }
  gfx::Point position = dialog_host->GetDialogPosition(size);
  // Align the first row of pixels inside the border. This is the apparent top
  // of the dialog.
  position.set_y(position.y() -
                 widget->non_client_view()->frame_view()->GetInsets().top());

  // Position the dialog below the top control of current window.
  gfx::Rect dialog_bounds(position, size);
  gfx::Rect dialog_screen_bounds =
      dialog_bounds +
      host_widget->GetClientAreaBoundsInScreen().OffsetFromOrigin();

  // Adjust the dialog bound to ensure it remains visible on the display.
  const gfx::Rect display_work_area =
      display::Screen::GetScreen()
          ->GetDisplayNearestView(dialog_host->GetHostView())
          .work_area();
  if (!display_work_area.Contains(dialog_screen_bounds)) {
    dialog_screen_bounds.AdjustToFit(display_work_area);
  }

  dialog_bounds.set_origin(dialog_screen_bounds.origin());
  widget->SetBounds(dialog_bounds);
}

gfx::NativeView GetParentView(content::WebContents* web_contents) {
  gfx::NativeView parent = gfx::NativeView();
  if (web_contents) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
        web_contents->GetTopLevelNativeWindow());
    DCHECK(widget) << "Could not find a parent widget!";
    if (widget) {
      parent = widget->GetNativeView();
    }
  }
  return parent;
}

views::Widget::InitParams CreateParams() {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.remove_standard_frame = true;
  params.type = views::Widget::InitParams::Type::TYPE_BUBBLE;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  return params;
}
}  // namespace

namespace commerce {

DialogArgs::DialogArgs(std::vector<GURL> urls,
                       std::string name,
                       std::string set_id,
                       bool in_new_tab)
    : urls(std::move(urls)),
      name(std::move(name)),
      set_id(std::move(set_id)),
      in_new_tab(in_new_tab) {}
DialogArgs::~DialogArgs() = default;
DialogArgs::DialogArgs(const DialogArgs&) = default;
DialogArgs& DialogArgs::operator=(const DialogArgs&) = default;

base::Value::Dict DialogArgs::ToValue() {
  base::Value::Dict dialog_args_value;
  base::Value::List product_spec_urls;
  for (auto url : urls) {
    product_spec_urls.Append(url.spec());
  }
  dialog_args_value.Set(kDialogArgsName, std::move(name));
  dialog_args_value.Set(kDialogArgsUrls, std::move(product_spec_urls));
  dialog_args_value.Set(kDialogArgsSetId, std::move(set_id));
  dialog_args_value.Set(kDialogArgsInNewTab, in_new_tab);
  return dialog_args_value;
}

// static
ProductSpecificationsDisclosureDialog*
    ProductSpecificationsDisclosureDialog::current_instance_ = nullptr;

// static
void ProductSpecificationsDisclosureDialog::ShowDialog(
    content::BrowserContext* browser_context,
    content::WebContents* web_contents,
    DialogArgs dialog_args) {
  if (current_instance_) {
    current_instance_->dialog_widget_->Close();
    current_instance_ = nullptr;
  }
  // ShowWebDialogWithParams() will take care of ownership.
  current_instance_ =
      new ProductSpecificationsDisclosureDialog(web_contents, dialog_args);
  gfx::NativeWindow dialog_window = chrome::ShowWebDialogWithParams(
      GetParentView(web_contents), browser_context, current_instance_,
      std::make_optional<views::Widget::InitParams>(CreateParams()));
  current_instance_->dialog_widget_ =
      views::Widget::GetWidgetForNativeWindow(dialog_window);

  // Move it to the top of the screen below omnibox. Default behavior is to show
  // it in the middle of the screen.
  UpdateDialogPosition(current_instance_->dialog_widget_, web_contents);
}

// static
bool ProductSpecificationsDisclosureDialog::CloseDialog() {
  if (current_instance_) {
    current_instance_->dialog_widget_->Close();
    current_instance_ = nullptr;
    return true;
  }
  return false;
}

ProductSpecificationsDisclosureDialog::ProductSpecificationsDisclosureDialog(
    content::WebContents* contents,
    DialogArgs dialog_args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  set_dialog_content_url(GURL(kChromeUICompareDisclosureUrl));
  set_dialog_frame_kind(FrameKind::kDialog);
  set_show_close_button(false);
  set_show_dialog_title(false);
  set_dialog_size(kDialogSize);
  set_can_close(true);
  set_allow_default_context_menu(false);
  set_dialog_modal_type(ui::mojom::ModalType::kNone);
  set_dialog_args(base::WriteJson(dialog_args.ToValue()).value());
}

ProductSpecificationsDisclosureDialog::
    ~ProductSpecificationsDisclosureDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  current_instance_ = nullptr;
}

}  // namespace commerce
