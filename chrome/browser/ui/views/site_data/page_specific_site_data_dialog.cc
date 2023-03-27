// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"
#include <memory>
#include <string>

#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/browser/ui/views/site_data/site_data_row_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "url/origin.h"

namespace {

struct PageSpecificSiteDataDialogSection {
  std::u16string title;
  std::u16string subtitle;
  std::vector<PageSpecificSiteDataDialogSite> sites;
  ui::ElementIdentifier identifier;
};

int GetContentSettingRowOrder(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return 0;
    case CONTENT_SETTING_SESSION_ONLY:
      return 1;
    case CONTENT_SETTING_BLOCK:
      return 2;
    default:
      NOTREACHED_NORETURN();
  }
}

// Creates a new CookiesTreeModel for all objects in the container,
// copying each of them.
std::unique_ptr<CookiesTreeModel> CreateCookiesTreeModel(
    const browsing_data::LocalSharedObjectsContainer& shared_objects) {
  return std::make_unique<CookiesTreeModel>(
      LocalDataContainer::CreateFromLocalSharedObjectsContainer(shared_objects),
      /*special_storage_policy=*/nullptr);
}

// Returns the registable domain (eTLD+1) for the |origin|. If it doesn't exist,
// returns the host.
std::string GetEtldPlusOne(const url::Origin& origin) {
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
    std::vector<PageSpecificSiteDataDialogSite> all_sites,
    const url::Origin& current_origin) {
  // TODO(crbug.com/1344787): Use actual strings.
  auto eltd_current_origin = GetEtldPlusOne(current_origin);

  PageSpecificSiteDataDialogSection first_party_section;
  first_party_section.title = l10n_util::GetStringUTF16(
      IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_FIRST_PARTY_TITLE);
  first_party_section.subtitle = l10n_util::GetStringUTF16(
      IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_FIRST_PARTY_SUBTITLE);
  first_party_section.identifier = kPageSpecificSiteDataDialogFirstPartySection;

  PageSpecificSiteDataDialogSection third_party_section;
  third_party_section.title = l10n_util::GetStringUTF16(
      IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_THIRD_PARTY_TITLE);
  third_party_section.subtitle = l10n_util::GetStringUTF16(
      IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_THIRD_PARTY_SUBTITLE);
  third_party_section.identifier = kPageSpecificSiteDataDialogThirdPartySection;

  for (const auto& site : all_sites) {
    if (GetEtldPlusOne(site.origin) == eltd_current_origin) {
      first_party_section.sites.push_back(site);
    } else {
      third_party_section.sites.push_back(site);
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
    cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
    host_content_settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(profile);

    RecordPageSpecificSiteDataDialogAction(
        PageSpecificSiteDataDialogAction::kDialogOpened);
  }

  void OnDialogExplicitlyClosed() {
    // If the user closes our parent tab while we're still open, this method
    // will (eventually) be called in response to a WebContentsDestroyed() call
    // from the WebContentsImpl to its observers.  But since the
    // infobars::ContentInfoBarManager is also torn down in response to
    // WebContentsDestroyed(), it may already be null. Since the tab is going
    // away anyway, we can just omit showing an infobar, which prevents any
    // attempt to access a null infobars::ContentInfoBarManager. Same applies to
    // removing the webcontents' user data.
    if (!web_contents_ || web_contents_->IsBeingDestroyed())
      return;

    if (status_changed_) {
      CollectedCookiesInfoBarDelegate::Create(
          infobars::ContentInfoBarManager::FromWebContents(
              web_contents_.get()));
    }

    // Reset the dialog reference in the user data. If the dialog is opened
    // again, a new instance should be created. When the dialog is destroyed
    // because of the web contents being destroyed, no need to remove the user
    // data because it will be destroyed.
    web_contents_->RemoveUserData(
        PageSpecificSiteDataDialogController::UserDataKey());
  }

  void SetBrowsingDataModelsForTesting(BrowsingDataModel* allowed,  // IN-TEST
                                       BrowsingDataModel* blocked) {
    allowed_browsing_data_model_for_testing_ = allowed;
    blocked_browsing_data_model_for_testing_ = blocked;
  }

  std::vector<PageSpecificSiteDataDialogSite> GetAllSites() {
    std::map<std::string, PageSpecificSiteDataDialogSite> sites_map;
    for (const std::unique_ptr<CookieTreeNode>& node :
         allowed_cookies_tree_model_->GetRoot()->children()) {
      std::string host_name = node->GetDetailedInfo().origin.host();
      auto existing_site = sites_map.find(host_name);
      if (existing_site == sites_map.end()) {
        sites_map.emplace(
            host_name,
            CreateSiteFromHostNode(node.get(), /*from_allowed_tree=*/true));
      } else {
        // To display the result entry as fully partitioned, both entries have
        // to be partitioned.
        existing_site->second.is_fully_partitioned &=
            IsCookieTreeNodeFullyPartitioned(node.get());
      }
      sites_map.emplace(
          node->GetDetailedInfo().origin.host(),
          CreateSiteFromHostNode(node.get(), /*from_allowed_tree=*/true));
    }
    for (const BrowsingDataModel::BrowsingDataEntryView& entry :
         *allowed_browsing_data_model()) {
      auto existing_site = sites_map.find(*entry.primary_host);
      if (existing_site == sites_map.end()) {
        sites_map.emplace(
            *entry.primary_host,
            CreateSiteFromEntryView(entry, /*from_allowed_model=*/true));
      } else {
        // To display the result entry as fully partitioned, entries from both
        // models have to be partitioned.
        existing_site->second.is_fully_partitioned &=
            IsBrowsingDataEntryViewFullyPartitioned(entry);
      }
    }
    for (const std::unique_ptr<CookieTreeNode>& node :
         blocked_cookies_tree_model_->GetRoot()->children()) {
      auto existing_site =
          sites_map.find(node->GetDetailedInfo().origin.host());
      // If there are multiple entries from the same tree, ignore the entry from
      // the blocked tree. It might be caused by partitioned allowed cookies and
      // regular blocked cookies or by cookies being set after creating an
      // exception and not reloading the page. Existing site entries doesn't
      // need to be updated as partitioned state isn't relevant for blocked
      // entries.
      if (existing_site == sites_map.end()) {
        sites_map.emplace(
            node->GetDetailedInfo().origin.host(),
            CreateSiteFromHostNode(node.get(), /*from_allowed_tree=*/false));
      }
    }
    for (const BrowsingDataModel::BrowsingDataEntryView& entry :
         *blocked_browsing_data_model()) {
      auto existing_site = sites_map.find(*entry.primary_host);
      if (existing_site == sites_map.end()) {
        // If there are multiple entries from the same tree, ignore the entry
        // from the blocked tree. It might be caused by partitioned allowed
        // cookies and regular blocked cookies or by cookies being set after
        // creating an exception and not reloading the page. Existing site
        // entries doesn't need to be updated as partitioned state isn't
        // relevant for blocked entries.
        sites_map.emplace(
            *entry.primary_host,
            CreateSiteFromEntryView(entry, /*from_allowed_model=*/false));
      }
    }

    std::vector<PageSpecificSiteDataDialogSite> sites;
    for (auto site : sites_map)
      sites.push_back(site.second);

    std::sort(sites.begin(), sites.end(), [](const auto& o1, const auto& o2) {
      int o1_order = GetContentSettingRowOrder(o1.setting);
      int o2_order = GetContentSettingRowOrder(o2.setting);
      if (o1_order != o2_order) {
        return o1_order < o2_order;
      }

      // Sort sites with the same content setting alphabetically.
      return o1.origin.host() < o2.origin.host();
    });

    return sites;
  }

  FaviconCache* favicon_cache() { return favicon_cache_.get(); }

  void DeleteStoredObjects(const url::Origin& origin) {
    status_changed_ = true;

    // The both models have to checked, as the site might be in the blocked
    // model, then be allowed and deleted. Without reloading the page the site
    // will remain in the blocked model.
    DeleteMatchingHostNodeFromModel(allowed_cookies_tree_model_.get(), origin);
    DeleteMatchingHostNodeFromModel(blocked_cookies_tree_model_.get(), origin);

    // Correctly remove partitioned storage (needs to be done separately), since
    // the existing calls don't apply the necessary filtering.
    DeletePartitionedStorage(origin);

    // Removing origin from Browsing Data Model to support new storage types.
    // The UI assumes deletion completed successfully, so we're passing
    // `base::DoNothing` callback.
    // TODO(crbug.com/1394352): Future tests will need to know when the deletion
    // is completed, this will require a callback to be passed here.

    allowed_browsing_data_model()->RemoveBrowsingData(origin.host(),
                                                      base::DoNothing());
    blocked_browsing_data_model()->RemoveBrowsingData(origin.host(),
                                                      base::DoNothing());

    RecordPageSpecificSiteDataDialogAction(
        PageSpecificSiteDataDialogAction::kSiteDeleted);
  }

  void SetContentException(const url::Origin& origin, ContentSetting setting) {
    status_changed_ = true;
    DCHECK(setting == CONTENT_SETTING_ALLOW ||
           setting == CONTENT_SETTING_BLOCK ||
           setting == CONTENT_SETTING_SESSION_ONLY);
    GURL url = origin.GetURL();
    if (CanCreateContentException(url)) {
      cookie_settings_->ResetCookieSetting(url);
      cookie_settings_->SetCookieSetting(url, setting);
    }
    RecordPageSpecificSiteDataDialogAction(
        GetDialogActionForContentSetting(setting));
  }

 private:
  // Deletes the host node matching |origin| and all stored objects for it.
  void DeleteMatchingHostNodeFromModel(CookiesTreeModel* model,
                                       const url::Origin& origin) {
    CookieTreeNode* node_to_delete = nullptr;
    for (const auto& node : model->GetRoot()->children()) {
      if (origin == node->GetDetailedInfo().origin) {
        DCHECK(!node_to_delete)
            << "The node with a matching origin should only be found once";
        node_to_delete = node.get();
      }
    }
    if (node_to_delete) {
      DCHECK_EQ(node_to_delete->GetDetailedInfo().node_type,
                CookieTreeNode::DetailedInfo::TYPE_HOST);
      model->DeleteCookieNode(node_to_delete);
    }
  }

  // TODO(crbug.com/1405808): Add an end-to-end browser test for this.
  void DeletePartitionedStorage(const url::Origin& origin) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    content::BrowsingDataRemover* remover = profile->GetBrowsingDataRemover();
    auto filter = content::BrowsingDataFilterBuilder::Create(
        content::BrowsingDataFilterBuilder::Mode::kDelete,
        content::BrowsingDataFilterBuilder::OriginMatchingMode::
            kOriginInAllContexts);
    filter->AddOrigin(origin);
    remover->RemoveWithFilter(
        base::Time::Min(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter));
  }

  bool CanCreateContentException(GURL url) const { return !url.SchemeIsFile(); }

  PageSpecificSiteDataDialogSite CreateSiteFromHostNode(
      CookieTreeNode* node,
      bool from_allowed_tree) {
    url::Origin origin = node->GetDetailedInfo().origin;
    return CreateSite(
        origin, from_allowed_tree,
        from_allowed_tree && IsCookieTreeNodeFullyPartitioned(node));
  }

  PageSpecificSiteDataDialogSite CreateSiteFromEntryView(
      const BrowsingDataModel::BrowsingDataEntryView& entry,
      bool from_allowed_model) {
    GURL current_url = web_contents_->GetVisibleURL();
    // TODO(crbug.com/1271155): BDM provides host name only while
    // CookieTreeModel provides url::Origin. This classes works with
    // url::Origin, so here we convert host name to origin with some assumptions
    // (which might not be true). We should either convert to work only with
    // host names or BDM should return origins.
    GURL site_url = net::cookie_util::CookieOriginToURL(
        *entry.primary_host, current_url.SchemeIsCryptographic());
    return CreateSite(url::Origin::Create(site_url), from_allowed_model,
                      IsBrowsingDataEntryViewFullyPartitioned(entry));
  }

  bool IsBrowsingDataEntryViewFullyPartitioned(
      const BrowsingDataModel::BrowsingDataEntryView& entry) {
    // TODO(crbug.com/1378703): Implement showing partitioned state from
    // BrowsingDataModel.
    return false;
  }

  bool IsCookieTreeNodeFullyPartitioned(CookieTreeNode* node) {
    GURL current_url = web_contents_->GetVisibleURL();
    url::Origin origin = node->GetDetailedInfo().origin;
    // TODO(crbug.com/1344787): Add a test to verify partitioned logic.
    // TODO(crbug.com/1271155): Consider reporting partitioned storage access
    // directly and remove this.
    return GetEtldPlusOne(origin) !=
               GetEtldPlusOne(url::Origin::Create(current_url)) &&
           IsOnlyPartitionedStorageAccessAllowed(origin) &&
           AreAllCookiesPartitioned(node);
  }

  PageSpecificSiteDataDialogSite CreateSite(url::Origin origin,
                                            bool from_allowed_model,
                                            bool is_fully_partitioned) {
    PageSpecificSiteDataDialogSite site;
    site.origin = origin;
    if (from_allowed_model) {
      // If the entry is in the allowed model, it should have either allowed or
      // clear-on-exit state. If the dialog is reopened after making changes but
      // before reloading the page, it will show the state of accesses on the
      // page load.
      site.setting = cookie_settings_->IsCookieSessionOnly(site.origin.GetURL())
                         ? CONTENT_SETTING_SESSION_ONLY
                         : CONTENT_SETTING_ALLOW;
    } else {
      site.setting = CONTENT_SETTING_BLOCK;
    }
    site.is_fully_partitioned = is_fully_partitioned;
    // TODO(crbug.com/1344787): Handle sources other than SETTING_SOURCE_USER.
    return site;
  }

  bool IsOnlyPartitionedStorageAccessAllowed(url::Origin site_origin) {
    GURL current_url = web_contents_->GetVisibleURL();

    const bool block_third_party_cookies =
        cookie_settings_->ShouldBlockThirdPartyCookies();
    const auto default_content_setting =
        cookie_settings_->GetDefaultCookieSetting(/*provider_id=*/nullptr);
    ContentSetting first_party_setting =
        host_content_settings_map_->GetContentSetting(
            current_url, GURL(), ContentSettingsType::COOKIES);

    content_settings::SettingInfo info;
    const base::Value value = host_content_settings_map_->GetWebsiteSetting(
        site_origin.GetURL(), current_url, ContentSettingsType::COOKIES, &info);

    bool has_site_level_exception =
        info.primary_pattern != ContentSettingsPattern::Wildcard() ||
        info.secondary_pattern != ContentSettingsPattern::Wildcard();

    // Partitioned access is displayed when all of these conditions are met:
    return
        // * third-party cookies are blocked
        block_third_party_cookies
        // * other cookies are allowed
        && default_content_setting != ContentSetting::CONTENT_SETTING_BLOCK
        // * there is no site level exception (the exception affects full cookie
        // access)
        && !has_site_level_exception
        // * first-party cookies are allowed (because partitioned cookies are
        // considered first party cookies, if first party is blocked from
        // accessing storage, partitioned cookies are too)
        && first_party_setting != CONTENT_SETTING_BLOCK;
  }

  bool AreAllCookiesPartitioned(CookieTreeNode* node) {
    bool all_partitioned = true;
    for (const auto& storage_type_node : node->children()) {
      if (storage_type_node->GetDetailedInfo().node_type !=
          CookieTreeNode::DetailedInfo::TYPE_COOKIES) {
        all_partitioned = false;
        break;
      }

      for (const auto& cookie_node : storage_type_node->children()) {
        if (!cookie_node->GetDetailedInfo().cookie->IsPartitioned()) {
          all_partitioned = false;
          break;
        }
      }
    }

    return all_partitioned;
  }

  BrowsingDataModel* allowed_browsing_data_model() {
    if (allowed_browsing_data_model_for_testing_) {
      return allowed_browsing_data_model_for_testing_;
    }

    auto* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            web_contents_->GetPrimaryMainFrame());
    return content_settings->allowed_browsing_data_model();
  }

  BrowsingDataModel* blocked_browsing_data_model() {
    if (blocked_browsing_data_model_for_testing_) {
      return blocked_browsing_data_model_for_testing_;
    }

    auto* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            web_contents_->GetPrimaryMainFrame());
    return content_settings->blocked_browsing_data_model();
  }

  base::WeakPtr<content::WebContents> web_contents_;
  // Each model represent separate local storage container. The implementation
  // doesn't make a difference between allowed and blocked models and checks
  // the actual content settings to determine the state.
  std::unique_ptr<CookiesTreeModel> allowed_cookies_tree_model_;
  std::unique_ptr<CookiesTreeModel> blocked_cookies_tree_model_;
  raw_ptr<BrowsingDataModel> allowed_browsing_data_model_for_testing_ = nullptr;
  raw_ptr<BrowsingDataModel> blocked_browsing_data_model_for_testing_ = nullptr;
  std::unique_ptr<FaviconCache> favicon_cache_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;

  // Whether user has done any changes to the site data, deleted site data for a
  // site or created a content setting exception for a site.
  bool status_changed_ = false;
};

class PageSpecificSiteDataSectionView : public views::BoxLayoutView {
 public:
  PageSpecificSiteDataSectionView(
      Profile* profile,
      std::vector<PageSpecificSiteDataDialogSite> sites,
      PageSpecificSiteDataDialogModelDelegate* delegate) {
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch);

    for (const PageSpecificSiteDataDialogSite& site : sites) {
      // It is safe to use base::Unretained for the delegate here because both
      // the row view and the delegate are owned by the dialog and will be
      // destroyed when the dialog is destroyed.
      auto* const row_view = AddChildView(std::make_unique<SiteDataRowView>(
          profile, site.origin, site.setting, site.is_fully_partitioned,
          delegate->favicon_cache(),
          base::BindRepeating(
              &PageSpecificSiteDataDialogModelDelegate::DeleteStoredObjects,
              base::Unretained(delegate)),
          base::BindRepeating(
              &PageSpecificSiteDataDialogModelDelegate::SetContentException,
              base::Unretained(delegate))));
      row_view->SetProperty(views::kElementIdentifierKey,
                            kPageSpecificSiteDataDialogRow);
    }

    empty_state_label_ = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(
            IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_EMPTY_STATE_LABEL),
        views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    empty_state_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    empty_state_label_->SetProperty(views::kElementIdentifierKey,
                                    kPageSpecificSiteDataDialogEmptyStateLabel);

    // Set insets to match with other views in the dialog.
    auto dialog_insets = ChromeLayoutProvider::Get()->GetInsetsMetric(
        views::InsetsMetric::INSETS_DIALOG);
    dialog_insets.set_top(0);
    dialog_insets.set_bottom(0);
    empty_state_label_->SetProperty(views::kMarginsKey, dialog_insets);

    UpdateEmptyStateLabelVisibility();
  }

  // views::View:
  void ChildVisibilityChanged(views::View* child) override {
    UpdateEmptyStateLabelVisibility();
  }

 private:
  void UpdateEmptyStateLabelVisibility() {
    // If none of the children (except the empty state label) are visible, show
    // a label to explain the empty state.
    bool none_children_visible =
        base::ranges::none_of(children(), [=](views::View* v) {
          return v != empty_state_label_ && v->GetVisible();
        });
    empty_state_label_->SetVisible(none_children_visible);
  }

  raw_ptr<views::Label> empty_state_label_ = nullptr;
};

}  // namespace

namespace test {

PageSpecificSiteDataDialogTestApi::PageSpecificSiteDataDialogTestApi(
    content::WebContents* web_contents)
    : delegate_(std::make_unique<PageSpecificSiteDataDialogModelDelegate>(
          web_contents)) {}

PageSpecificSiteDataDialogTestApi::~PageSpecificSiteDataDialogTestApi() =
    default;

void PageSpecificSiteDataDialogTestApi::SetBrowsingDataModels(
    BrowsingDataModel* allowed,
    BrowsingDataModel* blocked) {
  delegate_->SetBrowsingDataModelsForTesting(allowed, blocked);  // IN-TEST
}

std::vector<PageSpecificSiteDataDialogSite>
PageSpecificSiteDataDialogTestApi::GetAllSites() {
  return delegate_->GetAllSites();
}

void PageSpecificSiteDataDialogTestApi::DeleteStoredObjects(
    const url::Origin& origin) {
  delegate_->DeleteStoredObjects(origin);
}

}  // namespace test

DEFINE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogRow);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogFirstPartySection);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogThirdPartySection);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogEmptyStateLabel);

// static
views::Widget* ShowPageSpecificSiteDataDialog(
    content::WebContents* web_contents) {
  auto delegate_unique =
      std::make_unique<PageSpecificSiteDataDialogModelDelegate>(web_contents);
  PageSpecificSiteDataDialogModelDelegate* delegate = delegate_unique.get();
  auto builder = ui::DialogModel::Builder(std::move(delegate_unique));
  builder
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_TITLE))
      .SetInternalName("PageSpecificSiteDataDialog")
      .AddOkButton(
          base::BindRepeating(&PageSpecificSiteDataDialogModelDelegate::
                                  OnDialogExplicitlyClosed,
                              base::Unretained(delegate)),
          ui::DialogModelButton::Params().SetLabel(
              l10n_util::GetStringUTF16(IDS_DONE)))
      .SetCloseActionCallback(base::BindOnce(
          &PageSpecificSiteDataDialogModelDelegate::OnDialogExplicitlyClosed,
          base::Unretained(delegate)));

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bool has_any_sections = false;
  auto sections =
      GetSections(delegate->GetAllSites(),
                  url::Origin::Create(web_contents->GetVisibleURL()));
  for (const auto& section : sections) {
    // If section doesn't have any sites, don't show the section.
    if (section.sites.size() == 0u)
      continue;

    has_any_sections = true;
    builder.AddParagraph(
        ui::DialogModelLabel(section.subtitle).set_is_secondary(),
        section.title);
    builder.AddCustomField(
        CreateCustomField(std::make_unique<PageSpecificSiteDataSectionView>(
            profile, section.sites, delegate)),
        section.identifier);
  }

  // If there were no sections shown, show a label that explains an empty state.
  if (!has_any_sections) {
    builder.AddParagraph(
        ui::DialogModelLabel(
            l10n_util::GetStringUTF16(
                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_EMPTY_STATE_LABEL))
            .set_is_secondary());
  }

  return constrained_window::ShowWebModal(builder.Build(), web_contents);
}
