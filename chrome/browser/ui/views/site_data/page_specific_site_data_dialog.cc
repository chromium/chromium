// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"

#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/browser/ui/views/site_data/site_data_row_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

namespace {

struct PageSpecificSiteDataDialogSection {
  std::u16string title;
  std::u16string subtitle;
  std::vector<url::Origin> origins;
};

// Creates a new CookiesTreeModel for all objects in the container,
// copying each of them.
std::unique_ptr<CookiesTreeModel> CreateCookiesTreeModel(
    const browsing_data::LocalSharedObjectsContainer& shared_objects) {
  auto container = std::make_unique<LocalDataContainer>(
      shared_objects.cookies(), shared_objects.databases(),
      shared_objects.local_storages(), shared_objects.session_storages(),
      shared_objects.indexed_dbs(), shared_objects.file_systems(), nullptr,
      shared_objects.service_workers(), shared_objects.shared_workers(),
      shared_objects.cache_storages());

  return std::make_unique<CookiesTreeModel>(std::move(container), nullptr);
}

// Returns the registable domain (eTLD+1) for the |origin|. If it doesn't exist,
// returns the host.
std::string GetEtldPlusOne(url::Origin origin) {
  auto eltd_plus_one = net::registry_controlled_domains::GetDomainAndRegistry(
      origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return eltd_plus_one.empty() ? origin.host() : eltd_plus_one;
}

// Returns sections for the site data dialog. A section consists of a title, a
// subtitle and a list of rows. Each row represent an origin that has access to
// the site data or was blocked from accessing the site data.
// There are two sections:
// * "From this site" with origins that are in the same party as the
// |current_origin|.
// * "From other sites" with origins that are third parties in relation to the
// |current_origin|.
std::vector<PageSpecificSiteDataDialogSection> GetSections(
    std::vector<url::Origin> all_origins,
    const url::Origin& current_origin) {
  // TODO(crbug.com/1344787): Use actual strings.
  auto eltd_current_origin = GetEtldPlusOne(current_origin);

  PageSpecificSiteDataDialogSection first_party_section;
  first_party_section.title = u"From this site";
  first_party_section.subtitle = u"From this site subtitle";

  PageSpecificSiteDataDialogSection third_party_section;
  third_party_section.title = u"From other site";
  third_party_section.subtitle = u"From other site subtitle";

  for (const auto& origin : all_origins) {
    if (GetEtldPlusOne(origin) == eltd_current_origin) {
      first_party_section.origins.push_back(origin);
    } else {
      third_party_section.origins.push_back(origin);
    }
  }

  return {first_party_section, third_party_section};
}

// Creates a custom field for the dialog model. Behaves like a wrapper for a
// custom view and allows to add custom views to the dialog model.
std::unique_ptr<views::BubbleDialogModelHost::CustomView> CreateCustomField(
    std::unique_ptr<views::View> view) {
  return std::make_unique<views::BubbleDialogModelHost::CustomView>(
      std::move(view), views::BubbleDialogModelHost::FieldType::kMenuItem);
}

class PageSpecificSiteDataDialogModelDelegate : public ui::DialogModelDelegate {
 public:
  explicit PageSpecificSiteDataDialogModelDelegate(
      content::WebContents* web_contents)
      : web_contents_(web_contents->GetWeakPtr()) {
    auto* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            web_contents->GetPrimaryMainFrame());
    allowed_cookies_tree_model_ = CreateCookiesTreeModel(
        content_settings->allowed_local_shared_objects());
    blocked_cookies_tree_model_ = CreateCookiesTreeModel(
        content_settings->blocked_local_shared_objects());

    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    favicon_cache_ = std::make_unique<FaviconCache>(
        FaviconServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS));
  }

  void OnDialogExplicitlyClosed() {
    // Reset the dialog reference in the user data. If the dialog is opened
    // again, a new instance should be created. When the dialog is destroyed
    // because of the web contents being destroyed, no need to remove the user
    // data because it will be destroyed.
    if (web_contents_) {
      web_contents_->RemoveUserData(
          PageSpecificSiteDataDialogController::UserDataKey());
    }
  }

  std::vector<url::Origin> GetAllOrigins() {
    std::vector<url::Origin> all_origins;
    for (const auto& node :
         allowed_cookies_tree_model_->GetRoot()->children()) {
      all_origins.push_back(node->GetDetailedInfo().origin);
    }
    for (const auto& node :
         blocked_cookies_tree_model_->GetRoot()->children()) {
      all_origins.push_back(node->GetDetailedInfo().origin);
    }
    return all_origins;
  }

  FaviconCache* favicon_cache() { return favicon_cache_.get(); }

 private:
  base::WeakPtr<content::WebContents> web_contents_;
  // Each model represent separate local storage container. The implementation
  // doesn't make a difference between allowed and blocked models and checks
  // the actual content settings to determine the state.
  std::unique_ptr<CookiesTreeModel> allowed_cookies_tree_model_;
  std::unique_ptr<CookiesTreeModel> blocked_cookies_tree_model_;
  std::unique_ptr<FaviconCache> favicon_cache_;
};

}  // namespace

// static
views::Widget* ShowPageSpecificSiteDataDialog(
    content::WebContents* web_contents) {
  auto delegate_unique =
      std::make_unique<PageSpecificSiteDataDialogModelDelegate>(web_contents);
  PageSpecificSiteDataDialogModelDelegate* delegate = delegate_unique.get();
  auto builder = ui::DialogModel::Builder(std::move(delegate_unique));
  builder
      .SetTitle(l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_DIALOG_TITLE))
      .SetInternalName("PageSpecificSiteDataDialog")
      .SetCloseActionCallback(base::BindOnce(
          &PageSpecificSiteDataDialogModelDelegate::OnDialogExplicitlyClosed,
          base::Unretained(delegate)));

  auto sections =
      GetSections(delegate->GetAllOrigins(),
                  url::Origin::Create(web_contents->GetVisibleURL()));
  for (const auto& section : sections) {
    builder.AddBodyText(ui::DialogModelLabel(section.title));
    builder.AddBodyText(ui::DialogModelLabel(section.subtitle));
    for (const auto& origin : section.origins) {
      // TODO(crbug.com/1344787): Get the actual state based on the cookie
      // setting.
      builder.AddCustomField(
          CreateCustomField(std::make_unique<SiteDataRowView>(
              origin, CONTENT_SETTING_BLOCK, delegate->favicon_cache())));
    }
  }
  // TODO(crbug.com/1344787): Build the rest of the dialog. Add action handling.
  return constrained_window::ShowWebModal(builder.Build(), web_contents);
}
