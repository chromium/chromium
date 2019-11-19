// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_remoting_dialog_view.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

namespace media_router {

// static
void MediaRemotingDialogView::GetPermission(content::WebContents* web_contents,
                                            PermissionCallback callback) {
  HideDialog();  // Close the previous dialog if it is still showing.

  DCHECK(web_contents);
  DCHECK(callback);

  // Check whether user has set the permission.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  PrefService* const pref_service = profile->GetPrefs();
  DCHECK(pref_service);
  const PrefService::Preference* pref =
      pref_service->FindPreference(::prefs::kMediaRouterMediaRemotingEnabled);
  if (pref && !pref->IsDefaultValue()) {
    std::move(callback).Run(pref->GetValue()->GetBool());
    return;
  }

  // Show dialog.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser) {
    std::move(callback).Run(false);
    return;
  }
  views::View* icon_view =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar()->cast_button();
  MediaRouterActionController* action_controller =
      MediaRouterUIService::Get(profile)->action_controller();

  instance_ = new MediaRemotingDialogView(
      icon_view, pref_service, action_controller, std::move(callback));
  views::Widget* widget =
      views::BubbleDialogDelegateView::CreateBubble(instance_);
  widget->Show();
}

// static
void MediaRemotingDialogView::HideDialog() {
  if (IsShowing())
    instance_->GetWidget()->Close();
  // We also set |instance_| to nullptr in WindowClosing() which is called
  // asynchronously, because not all paths to close the dialog go through
  // HideDialog(). We set it here because IsShowing() should be false after
  // HideDialog() is called.
  instance_ = nullptr;
}

// static
bool MediaRemotingDialogView::IsShowing() {
  return instance_ != nullptr;
}

bool MediaRemotingDialogView::ShouldShowCloseButton() const {
  return true;
}

base::string16 MediaRemotingDialogView::GetWindowTitle() const {
  return dialog_title_;
}

bool MediaRemotingDialogView::Accept() {
  ReportPermission(true);
  return true;
}

bool MediaRemotingDialogView::Cancel() {
  ReportPermission(false);
  return true;
}

bool MediaRemotingDialogView::Close() {
  return true;
}

gfx::Size MediaRemotingDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

MediaRemotingDialogView::MediaRemotingDialogView(
    views::View* anchor_view,
    PrefService* pref_service,
    MediaRouterActionController* action_controller,
    PermissionCallback callback)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      permission_callback_(std::move(callback)),
      pref_service_(pref_service),
      action_controller_(action_controller),
      dialog_title_(
          l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_REMOTING_DIALOG_TITLE)) {
  DCHECK(pref_service_);
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_ROUTER_REMOTING_DIALOG_OPTIMIZE_BUTTON));
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_ROUTER_REMOTING_DIALOG_CANCEL_BUTTON));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  // Depress the Cast toolbar icon.
  action_controller_->OnDialogShown();
}

MediaRemotingDialogView::~MediaRemotingDialogView() {
  action_controller_->OnDialogHidden();
}

void MediaRemotingDialogView::Init() {
  views::Label* body_text = new views::Label(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_REMOTING_DIALOG_BODY_TEXT),
      views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT, views::style::STYLE_PRIMARY);
  body_text->SetMultiLine(true);
  body_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(body_text);

  remember_choice_checkbox_ = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_REMOTING_DIALOG_CHECKBOX));
  AddChildView(remember_choice_checkbox_);
}

void MediaRemotingDialogView::WindowClosing() {
  if (instance_ == this)
    instance_ = nullptr;
}

void MediaRemotingDialogView::ReportPermission(bool allowed) {
  DCHECK(remember_choice_checkbox_);
  DCHECK(permission_callback_);
  if (remember_choice_checkbox_->GetChecked()) {
    pref_service_->SetBoolean(::prefs::kMediaRouterMediaRemotingEnabled,
                              allowed);
  }
  std::move(permission_callback_).Run(allowed);
}

// static
MediaRemotingDialogView* MediaRemotingDialogView::instance_ = nullptr;

}  // namespace media_router
