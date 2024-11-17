// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_remoting_dialog_view.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/toolbar/cast/cast_toolbar_button_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

namespace media_router {

MediaRemotingDialogCoordinatorViews::MediaRemotingDialogCoordinatorViews(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
}

MediaRemotingDialogCoordinatorViews::~MediaRemotingDialogCoordinatorViews() =
    default;

bool MediaRemotingDialogCoordinatorViews::Show(
    PermissionCallback permission_callback) {
  DCHECK(permission_callback);

  Hide();  // Close the previous dialog if it is still showing.

  Browser* const browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    std::move(permission_callback).Run(false);
    return false;
  }
  views::View* const icon_view = BrowserView::GetBrowserViewForBrowser(browser)
                                     ->toolbar()
                                     ->GetCastButton();
  Profile* const profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  PrefService* const pref_service = profile->GetPrefs();
  CastToolbarButtonController* const action_controller =
      MediaRouterUIService::Get(profile)->action_controller();

  auto remoting_dialog = std::make_unique<MediaRemotingDialogView>(
      icon_view, pref_service, action_controller,
      std::move(permission_callback));
  tracker_.SetView(remoting_dialog.get());
  views::BubbleDialogDelegateView::CreateBubble(std::move(remoting_dialog))
      ->Show();
  return true;
}

void MediaRemotingDialogCoordinatorViews::Hide() {
  if (IsShowing())
    tracker_.view()->GetWidget()->Close();
}

bool MediaRemotingDialogCoordinatorViews::IsShowing() const {
  return !!tracker_.view();
}

MediaRemotingDialogView::MediaRemotingDialogView(
    views::View* anchor_view,
    PrefService* pref_service,
    CastToolbarButtonController* action_controller,
    MediaRemotingDialogCoordinator::PermissionCallback callback)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      pref_service_(pref_service),
      action_controller_(action_controller),
      permission_callback_(std::move(callback)) {
  DCHECK(pref_service_);
  SetShowCloseButton(true);
  SetTitle(IDS_MEDIA_ROUTER_REMOTING_DIALOG_TITLE);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(
                     IDS_MEDIA_ROUTER_REMOTING_DIALOG_OPTIMIZE_BUTTON));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(
                     IDS_MEDIA_ROUTER_REMOTING_DIALOG_CANCEL_BUTTON));

  SetAcceptCallback(base::BindOnce(&MediaRemotingDialogView::ReportPermission,
                                   base::Unretained(this), true));
  SetCancelCallback(base::BindOnce(&MediaRemotingDialogView::ReportPermission,
                                   base::Unretained(this), false));

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

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
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_PRIMARY);
  body_text->SetMultiLine(true);
  body_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(body_text);

  remember_choice_checkbox_ = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_REMOTING_DIALOG_CHECKBOX));
  AddChildView(remember_choice_checkbox_.get());
}

void MediaRemotingDialogView::ReportPermission(bool allowed) {
  DCHECK(remember_choice_checkbox_);
  DCHECK(permission_callback_);
  if (remember_choice_checkbox_->GetChecked()) {
    pref_service_->SetBoolean(prefs::kMediaRouterMediaRemotingEnabled, allowed);
  }
  std::move(permission_callback_).Run(allowed);
}

BEGIN_METADATA(MediaRemotingDialogView)
END_METADATA

}  // namespace media_router
