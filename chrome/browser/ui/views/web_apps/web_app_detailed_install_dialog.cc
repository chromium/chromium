// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/views/web_apps/web_app_detailed_install_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_info_image_source.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/window/dialog_delegate.h"

namespace chrome {

void ShowWebAppDetailedInstallDialog(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> install_info,
    chrome::AppInstallationAcceptanceCallback callback,
    chrome::PwaInProductHelpState iph_state) {
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  PrefService* const prefs =
      Profile::FromBrowserContext(browser_context)->GetPrefs();

  feature_engagement::Tracker* const tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(browser_context);

  constexpr int kIconSize = 32;

  gfx::ImageSkia image(std::make_unique<WebAppInfoImageSource>(
                           kIconSize, install_info->icon_bitmaps.any),
                       gfx::Size(kIconSize, kIconSize));

  auto title = install_info->title;
  auto description = install_info->description;

  auto delegate =
      std::make_unique<web_app::WebAppDetailedInstallDialogDelegate>(
          web_contents, std::move(install_info), std::move(callback),
          std::move(iph_state), prefs, tracker);

  auto* delegate_ptr = delegate.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(delegate))
          .SetIcon(ui::ImageModel::FromImageSkia(image))
          .SetTitle(title)  // TODO(pbos): Add secondary-title support for
                            // base::UTF8ToUTF16(install_info->start_url.host())
          .AddBodyText(ui::DialogModelLabel(description))
          .AddOkButton(
              base::BindOnce(
                  &web_app::WebAppDetailedInstallDialogDelegate::OnAccept,
                  base::Unretained(delegate_ptr)),
              l10n_util::GetStringUTF16(IDS_INSTALL))
          .AddCancelButton(base::BindOnce(
              &web_app::WebAppDetailedInstallDialogDelegate::OnCancel,
              base::Unretained(delegate_ptr)))
          .Build();

  constrained_window::ShowWebModal(std::move(dialog_model), web_contents);
}

}  // namespace chrome

namespace web_app {

WebAppDetailedInstallDialogDelegate::WebAppDetailedInstallDialogDelegate(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    chrome::AppInstallationAcceptanceCallback callback,
    chrome::PwaInProductHelpState iph_state,
    PrefService* prefs,
    feature_engagement::Tracker* tracker)
    : web_contents_(web_contents),
      install_info_(std::move(web_app_info)),
      callback_(std::move(callback)),
      iph_state_(std::move(iph_state)),
      prefs_(prefs),
      tracker_(tracker) {
  DCHECK(install_info_);
  DCHECK(prefs_);
}

WebAppDetailedInstallDialogDelegate::~WebAppDetailedInstallDialogDelegate() {
  // TODO(crbug.com/1327363): move this to dialog->SetHighlightedButton.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser)
    return;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  // Dehighlight the install icon when this dialog is closed.
  browser_view->toolbar_button_provider()
      ->GetPageActionIconView(PageActionIconType::kPwaInstall)
      ->SetHighlighted(false);
}

void WebAppDetailedInstallDialogDelegate::OnAccept() {
  if (iph_state_ == chrome::PwaInProductHelpState::kShown) {
    web_app::AppId app_id = web_app::GenerateAppId(install_info_->manifest_id,
                                                   install_info_->start_url);
    web_app::RecordInstallIphInstalled(prefs_, app_id);
    tracker_->NotifyEvent(feature_engagement::events::kDesktopPwaInstalled);
  }

  std::move(callback_).Run(true, std::move(install_info_));
}

void WebAppDetailedInstallDialogDelegate::OnCancel() {
  if (iph_state_ == chrome::PwaInProductHelpState::kShown && install_info_) {
    web_app::AppId app_id = web_app::GenerateAppId(install_info_->manifest_id,
                                                   install_info_->start_url);
    web_app::RecordInstallIphIgnored(prefs_, app_id, base::Time::Now());
  }

  std::move(callback_).Run(false, std::move(install_info_));
}

}  // namespace web_app
