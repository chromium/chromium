// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/reload_button_web_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/webui/reload_button/reload_button_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom-data-view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

ReloadButtonWebView::ReloadButtonWebView(
    BrowserWindowInterface* browser,
    chrome::BrowserCommandController* controller)
    : controller_(controller) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto web_view = std::make_unique<views::WebView>(browser->GetProfile());
  const GURL kUrl(chrome::kChromeUIReloadButtonURL);
  auto* web_contents = web_view->GetWebContents(kUrl);
  // PLM has to be initialized before loading the URL.
  InitializePageLoadMetricsForWebContents(web_contents);
  web_view->LoadInitialURL(kUrl);
  const int size = GetLayoutConstant(LayoutConstant::TOOLBAR_BUTTON_HEIGHT);
  web_view->SetPreferredSize(gfx::Size(size, size));
  webui::SetBrowserWindowInterface(web_contents, browser);
  web_contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  reload_button_ui_ =
      web_contents->GetWebUI()->GetController()->GetAs<ReloadButtonUI>();
  web_view->SetID(VIEW_ID_RELOAD_BUTTON);
  AddChildView(std::move(web_view));
  web_contents->SetDelegate(this);

  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItemWithStringId(IDC_RELOAD,
                                   IDS_RELOAD_MENU_NORMAL_RELOAD_ITEM);
  menu_model_->AddItemWithStringId(IDC_RELOAD_BYPASSING_CACHE,
                                   IDS_RELOAD_MENU_HARD_RELOAD_ITEM);
  menu_model_->AddItemWithStringId(IDC_RELOAD_CLEARING_CACHE,
                                   IDS_RELOAD_MENU_EMPTY_AND_HARD_RELOAD_ITEM);

  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::CONTEXT_MENU);

  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD));
  GetViewAccessibility().SetDefaultActionVerb(
      ax::mojom::DefaultActionVerb::kPress);
  GetViewAccessibility().AddAction(ax::mojom::Action::kShowContextMenu);
  UpdateAccessibleHasPopup();
  UpdateTooltipText();
  SetProperty(views::kElementIdentifierKey, kReloadButtonElementId);
  SetReloadButtonUIState();
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
}

ReloadButtonWebView::~ReloadButtonWebView() = default;

void ReloadButtonWebView::ChangeMode(ReloadControl::Mode mode, bool force) {
  // TODO(crbug.com/444358999): Now the mode is always updated immediately from
  // the browser side, then a mojo IPC is sent to the renderer to make the
  // change accordingly. We may need to implement the timer/force updating logic
  // in the future.
  mode_ = mode;
  SetReloadButtonUIState();
  UpdateTooltipText();
}

views::View* ReloadButtonWebView::GetAsViewClassForTesting() {
  return this;
}

bool ReloadButtonWebView::GetMenuEnabled() const {
  return is_menu_enabled_;
}

void ReloadButtonWebView::SetMenuEnabled(bool is_menu_enabled) {
  is_menu_enabled_ = is_menu_enabled;
  UpdateAccessibleHasPopup();
  SetReloadButtonUIState();
  UpdateTooltipText();
}

bool ReloadButtonWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  if (is_menu_enabled_) {
    gfx::Point screen_location = GetBoundsInScreen().origin();
    screen_location.Offset(params.x, params.y);
    menu_runner_->RunMenuAt(
        GetWidget(), nullptr, gfx::Rect(screen_location, gfx::Size()),
        views::MenuAnchorPosition::kBubbleBottomRight, params.source_type);
  }
  return true;
}

bool ReloadButtonWebView::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ReloadButtonWebView::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ReloadButtonWebView::IsCommandIdVisible(int command_id) const {
  return true;
}

bool ReloadButtonWebView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

void ReloadButtonWebView::ExecuteCommand(int command_id, int event_flags) {
  controller_->ExecuteCommandWithDisposition(
      command_id, ui::DispositionFromEventFlags(event_flags));
}

void ReloadButtonWebView::UpdateAccessibleHasPopup() {
  GetViewAccessibility().SetHasPopup((is_menu_enabled_ && menu_model_)
                                         ? ax::mojom::HasPopup::kMenu
                                         : ax::mojom::HasPopup::kNone);
}

void ReloadButtonWebView::UpdateTooltipText() {
  SetTooltipText(l10n_util::GetStringUTF16(
      mode_ == ReloadControl::Mode::kReload
          ? (is_menu_enabled_ ? IDS_TOOLTIP_RELOAD_WITH_MENU
                              : IDS_TOOLTIP_RELOAD)
          : IDS_TOOLTIP_STOP));
}

void ReloadButtonWebView::SetReloadButtonUIState() {
  CHECK(reload_button_ui_);
  reload_button_ui_->SetReloadButtonState(
      /*is_loading=*/mode_ == ReloadControl::Mode::kStop, is_menu_enabled_);
}

BEGIN_METADATA(ReloadButtonWebView)
END_METADATA
