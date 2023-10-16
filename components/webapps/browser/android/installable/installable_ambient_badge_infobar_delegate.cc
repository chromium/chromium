// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/installable/installable_ambient_badge_infobar_delegate.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_infobar.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/webapps_client.h"
#include "ui/base/l10n/l10n_util.h"

namespace webapps {

InstallableAmbientBadgeInfoBarDelegate::
    ~InstallableAmbientBadgeInfoBarDelegate() = default;

// static
infobars::InfoBar*
InstallableAmbientBadgeInfoBarDelegate::GetVisibleAmbientBadgeInfoBar(
    infobars::ContentInfoBarManager* infobar_manager) {
  const auto it =
      base::ranges::find(infobar_manager->infobars(),
                         InstallableAmbientBadgeInfoBarDelegate::
                             INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE,
                         &infobars::InfoBar::GetIdentifier);
  return it != infobar_manager->infobars().cend() ? *it : nullptr;
}

// static
void InstallableAmbientBadgeInfoBarDelegate::Create(
    content::WebContents* web_contents,
    base::WeakPtr<InstallableAmbientBadgeClient> weak_client,
    const std::u16string& app_name,
    const SkBitmap& primary_icon,
    const bool is_primary_icon_maskable,
    const GURL& start_url) {
  auto* infobar_manager =
      WebappsClient::Get()->GetInfoBarManagerForWebContents(web_contents);
  if (infobar_manager == nullptr)
    return;

  infobar_manager->AddInfoBar(std::make_unique<InstallableAmbientBadgeInfoBar>(
      base::WrapUnique(new InstallableAmbientBadgeInfoBarDelegate(
          weak_client, app_name, primary_icon, is_primary_icon_maskable,
          start_url))));
}

void InstallableAmbientBadgeInfoBarDelegate::AddToHomescreen() {
  if (!weak_client_.get())
    return;

  weak_client_->AddToHomescreenFromBadge();
}

const std::u16string InstallableAmbientBadgeInfoBarDelegate::GetMessageText()
    const {
  if (!base::FeatureList::IsEnabled(features::kAddToHomescreenMessaging))
    return l10n_util::GetStringFUTF16(IDS_AMBIENT_BADGE_INSTALL, app_name_);

  bool include_no_download_required = base::GetFieldTrialParamByFeatureAsBool(
      features::kAddToHomescreenMessaging, "include_no_download_required",
      /* default_value= */ false);

  return l10n_util::GetStringFUTF16(
      include_no_download_required
          ? IDS_AMBIENT_BADGE_INSTALL_ALTERNATIVE_NO_DOWNLOAD_REQUIRED
          : IDS_AMBIENT_BADGE_INSTALL_ALTERNATIVE,
      app_name_);
}

const SkBitmap& InstallableAmbientBadgeInfoBarDelegate::GetPrimaryIcon() const {
  return primary_icon_;
}

bool InstallableAmbientBadgeInfoBarDelegate::GetIsPrimaryIconMaskable() const {
  return is_primary_icon_maskable_;
}

InstallableAmbientBadgeInfoBarDelegate::InstallableAmbientBadgeInfoBarDelegate(
    base::WeakPtr<InstallableAmbientBadgeClient> weak_client,
    const std::u16string& app_name,
    const SkBitmap& primary_icon,
    const bool is_primary_icon_maskable,
    const GURL& start_url)
    : infobars::InfoBarDelegate(),
      weak_client_(weak_client),
      app_name_(app_name),
      primary_icon_(primary_icon),
      is_primary_icon_maskable_(is_primary_icon_maskable),
      start_url_(start_url) {}

infobars::InfoBarDelegate::InfoBarIdentifier
InstallableAmbientBadgeInfoBarDelegate::GetIdentifier() const {
  return INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE;
}

void InstallableAmbientBadgeInfoBarDelegate::InfoBarDismissed() {
  if (!weak_client_.get())
    return;

  weak_client_->BadgeDismissed();
}

}  // namespace webapps
