// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_site_access_combobox_model.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

ExtensionSiteAccessComboboxModel::ExtensionSiteAccessComboboxModel(
    Browser* browser,
    const extensions::Extension* extension)
    : browser_(browser), extension_(extension) {
  items_.push_back(extensions::SitePermissionsHelper::SiteAccess::kOnClick);
  items_.push_back(extensions::SitePermissionsHelper::SiteAccess::kOnSite);
  items_.push_back(extensions::SitePermissionsHelper::SiteAccess::kOnAllSites);
}

ExtensionSiteAccessComboboxModel::~ExtensionSiteAccessComboboxModel() = default;

void ExtensionSiteAccessComboboxModel::HandleSelection(size_t new_index) {
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents || !ExtensionIsValid())
    return;
  DCHECK_LT(new_index, items_.size());

  LogSiteAccessAction(items_[new_index]);

  extensions::SitePermissionsHelper(browser_->profile())
      .UpdateSiteAccess(*extension_, web_contents, items_[new_index]);
}

size_t ExtensionSiteAccessComboboxModel::GetCurrentSiteAccessIndex() const {
  DCHECK(ExtensionIsValid());

  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  DCHECK(web_contents);

  extensions::SitePermissionsHelper::SiteAccess current_access =
      extensions::SitePermissionsHelper(browser_->profile())
          .GetSiteAccess(*extension_, web_contents->GetLastCommittedURL());
  auto item_it = base::ranges::find(items_, current_access);
  DCHECK(item_it != items_.end());

  return static_cast<size_t>(item_it - items_.begin());
}

size_t ExtensionSiteAccessComboboxModel::GetItemCount() const {
  return items_.size();
}

std::u16string ExtensionSiteAccessComboboxModel::GetItemAt(size_t index) const {
  int label_id = 0;
  switch (items_[index]) {
    case extensions::SitePermissionsHelper::SiteAccess::kOnClick:
      label_id = IDS_EXTENSIONS_MENU_SITE_ACCESS_COMBOBOX_RUN_ON_CLICK;
      break;
    case extensions::SitePermissionsHelper::SiteAccess::kOnSite:
      label_id = IDS_EXTENSIONS_MENU_SITE_ACCESS_COMBOBOX_RUN_ON_SITE;
      break;
    case extensions::SitePermissionsHelper::SiteAccess::kOnAllSites:
      label_id = IDS_EXTENSIONS_MENU_SITE_ACCESS_COMBOBOX_RUN_ON_ALL_SITES;
      break;
  }
  return l10n_util::GetStringUTF16(label_id);
}

absl::optional<size_t> ExtensionSiteAccessComboboxModel::GetDefaultIndex()
    const {
  return GetCurrentSiteAccessIndex();
}

bool ExtensionSiteAccessComboboxModel::IsItemEnabledAt(size_t index) const {
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents || !ExtensionIsValid())
    return false;

  return extensions::SitePermissionsHelper(browser_->profile())
      .CanSelectSiteAccess(*extension_, web_contents->GetLastCommittedURL(),
                           items_[index]);
}

bool ExtensionSiteAccessComboboxModel::ExtensionIsValid() const {
  return extensions::ExtensionRegistry::Get(browser_->profile())
      ->enabled_extensions()
      .Contains(extension_->id());
}

void ExtensionSiteAccessComboboxModel::LogSiteAccessAction(
    extensions::SitePermissionsHelper::SiteAccess site_access) const {
  switch (site_access) {
    case extensions::SitePermissionsHelper::SiteAccess::kOnClick:
      base::RecordAction(base::UserMetricsAction(
          "Extensions.Toolbar.SiteAccessCombobox.OnClickSelected"));
      break;
    case extensions::SitePermissionsHelper::SiteAccess::kOnSite:
      base::RecordAction(base::UserMetricsAction(
          "Extensions.Toolbar.SiteAccessCombobox.OnSiteSelected"));
      break;
    case extensions::SitePermissionsHelper::SiteAccess::kOnAllSites:
      base::RecordAction(base::UserMetricsAction(
          "Extensions.Toolbar.SiteAccessCombobox.OnAllSitesSelected"));
      break;
  }
}
