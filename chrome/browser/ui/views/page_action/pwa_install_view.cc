// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/pwa_install_view.h"

#include "base/bind_helpers.h"
#include "base/metrics/user_metrics.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/ui/views/extensions/pwa_confirmation_bubble_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

PwaInstallView::PwaInstallView(CommandUpdater* command_updater,
                               PageActionIconView::Delegate* delegate)
    : PageActionIconView(nullptr, 0, delegate) {
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_OMNIBOX_PWA_INSTALL_ICON_LABEL));
  SetUpForInOutAnimation();
}

PwaInstallView::~PwaInstallView() {}

bool PwaInstallView::Update() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;

  auto* manager = banners::AppBannerManager::FromWebContents(web_contents);
  // May not be present e.g. in incognito mode.
  if (!manager)
    return false;

  bool is_probably_promotable = manager->IsProbablyPromotableWebApp();
  if (is_probably_promotable && manager->MaybeConsumeInstallAnimation())
    AnimateIn(base::nullopt);
  else
    ResetSlideAnimation(false);

  bool was_visible = GetVisible();
  SetVisible(is_probably_promotable || PWAConfirmationBubbleView::IsShowing());
  return GetVisible() != was_visible;
}

void PwaInstallView::OnExecuting(PageActionIconView::ExecuteSource source) {
  base::RecordAction(base::UserMetricsAction("PWAInstallIcon"));
  web_app::CreateWebAppFromManifest(GetWebContents(),
                                    WebappInstallSource::OMNIBOX_INSTALL_ICON,
                                    base::DoNothing());
}

views::BubbleDialogDelegateView* PwaInstallView::GetBubble() const {
  // TODO(https://907351): Implement.
  return nullptr;
}

const gfx::VectorIcon& PwaInstallView::GetVectorIcon() const {
  return omnibox::kPlusIcon;
}

base::string16 PwaInstallView::GetTextForTooltipAndAccessibleName() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return base::string16();
  return l10n_util::GetStringFUTF16(
      IDS_OMNIBOX_PWA_INSTALL_ICON_TOOLTIP,
      banners::AppBannerManager::GetInstallableWebAppName(web_contents));
}
