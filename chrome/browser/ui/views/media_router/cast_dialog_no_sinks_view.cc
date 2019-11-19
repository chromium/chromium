// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_no_sinks_view.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "url/gurl.h"

namespace media_router {

CastDialogNoSinksView::CastDialogNoSinksView(Profile* profile)
    : profile_(profile) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  looking_for_sinks_view_ = CreateLookingForSinksView();
  AddChildView(looking_for_sinks_view_);

  base::PostDelayedTask(FROM_HERE, {content::BrowserThread::UI},
                        base::BindOnce(&CastDialogNoSinksView::ShowHelpIconView,
                                       weak_factory_.GetWeakPtr()),
                        base::TimeDelta::FromSeconds(3));
}

CastDialogNoSinksView::~CastDialogNoSinksView() = default;

void CastDialogNoSinksView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  // This is called when |help_icon_| is clicked.
  ShowHelpCenterArticle();
}

void CastDialogNoSinksView::ShowHelpIconView() {
  delete looking_for_sinks_view_;
  looking_for_sinks_view_ = nullptr;
  help_icon_view_ = CreateHelpIconView();
  AddChildView(help_icon_view_);
  Layout();
}

void CastDialogNoSinksView::ShowHelpCenterArticle() {
  const GURL url = GURL(chrome::kCastNoDestinationFoundURL);
  NavigateParams params(profile_, url, ui::PAGE_TRANSITION_LINK);
  Navigate(&params);
}

views::View* CastDialogNoSinksView::CreateLookingForSinksView() {
  base::string16 title =
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_STATUS_LOOKING_FOR_DEVICES);
  HoverButton* view = new HoverButton(
      /* button_listener */ nullptr, CreateThrobber(), title, base::string16());
  view->SetEnabled(false);
  return view;
}

views::View* CastDialogNoSinksView::CreateHelpIconView() {
  base::string16 title =
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_STATUS_NO_DEVICES_FOUND);
  auto help_icon = std::make_unique<views::ImageButton>(this);
  views::ImageButton* help_icon_ptr = help_icon.get();
  const SkColor icon_color = help_icon->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
  help_icon->SetInstallFocusRingOnFocus(true);
  help_icon->SetImage(views::Button::STATE_NORMAL,
                      gfx::CreateVectorIcon(::vector_icons::kHelpOutlineIcon,
                                            kPrimaryIconSize, icon_color));
  help_icon->SetFocusForPlatform();
  help_icon->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  help_icon->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_NO_DEVICES_FOUND_BUTTON));
  HoverButton* view =
      new HoverButton(/* button_listener */ nullptr, std::move(help_icon),
                      title, base::string16());
  view->SetEnabled(false);
  // HoverButton disables event handling by its icons, so enable it again.
  help_icon_ptr->set_can_process_events_within_subtree(true);
  return view;
}

}  // namespace media_router
