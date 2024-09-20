// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/metrics/user_metrics_action.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/browser/ui/views/site_data/related_app_row_view.h"
#include "chrome/browser/ui/views/site_data/site_data_row_view.h"
#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/strings/grit/components_strings.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/styled_label.h"
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
      NOTREACHED();
  }
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
  // TODO(crbug.com/40231917): Use actual strings.
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
    Profile* profile =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext());
    favicon_cache_ = std::make_unique<FaviconCache>(
        FaviconServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS));
    cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
    tracking_protection_settings_ =
        TrackingProtectionSettingsFactory::GetForProfile(profile);
    host_content_settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(profile);

    RecordPageSpecificSiteDataDialogOpenedAction();
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
    if (!web_contents_ || web_contents_->IsBeingDestroyed()) {
      return;
    }

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
    std::map<BrowsingDataModel::DataOwner, PageSpecificSiteDataDialogSite>
        sites_map;
    for (const BrowsingDataModel::BrowsingDataEntryView& entry :
         *allowed_browsing_data_model()) {
      const BrowsingDataModel::DataOwner& owner = *entry.data_owner;
      auto existing_site = sites_map.find(owner);
      if (existing_site == sites_map.end()) {
        sites_map.emplace(
            owner, CreateSiteFromEntryView(entry, /*from_allowed_model=*/true));
      } else {
        // To display the result entry as fully partitioned, entries from both
        // models have to be partitioned.
        existing_site->second.is_fully_partitioned &=
            IsBrowsingDataEntryViewFullyPartitioned(entry);
      }
    }
    for (const BrowsingDataModel::BrowsingDataEntryView& entry :
         *blocked_browsing_data_model()) {
      const BrowsingDataModel::DataOwner& owner = *entry.data_owner;
      auto existing_site = sites_map.find(owner);
      if (existing_site == sites_map.end()) {
        // If there are multiple entries from the same tree, ignore the entry
        // from the blocked tree. It might be caused by partitioned allowed
        // cookies and regular blocked cookies or by cookies being set after
        // creating an exception and not reloading the page. Existing site
        // entries doesn't need to be updated as partitioned state isn't
        // relevant for blocked entries.
        sites_map.emplace(owner, CreateSiteFromEntryView(
                                     entry, /*from_allowed_model=*/false));
      }
    }

    std::vector<PageSpecificSiteDataDialogSite> sites;
    for (auto site : sites_map) {
      sites.push_back(site.second);
    }

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

  std::vector<webapps::AppId> GetInstalledRelatedApps() {
    // There will be no installed related apps in off the record mode, because
    // apps are not installable there.
    if (web_contents_->GetBrowserContext()->IsOffTheRecord()) {
      return {};
    }

    // If the provider isn't ready, there will also be no apps installed.
    web_app::WebAppProvider* provider = web_app::WebAppProvider::GetForWebApps(
        Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
    if (!provider) {
      return {};
    }

    const GURL last_committed_url(
        web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL());
    // We don't need to lock the registrar, as we're only reading the list of
    // apps from it. See RelatedAppRowView::RelatedAppRowView() for more info.
    base::flat_map<webapps::AppId, std::string> all_controlling_apps_map =
        provider->registrar_unsafe().GetAllAppsControllingUrl(
            last_committed_url);
    std::vector<webapps::AppId> all_controlling_apps_list;
    all_controlling_apps_list.reserve(all_controlling_apps_map.size());
    for (const auto& [app_id, _] : all_controlling_apps_map) {
      all_controlling_apps_list.push_back(app_id);
    }
    return all_controlling_apps_list;
  }

  FaviconCache* favicon_cache() { return favicon_cache_.get(); }

  void DeleteStoredObjects(const url::Origin& origin) {
    status_changed_ = true;

    // The both models have to be checked, as the site might be in the blocked
    // model, then be allowed and deleted. Without reloading the page the site
    // will remain in the blocked model.
    // Removing origin from Browsing Data Model to support new storage types.
    // The UI assumes deletion completed successfully, so we're passing
    // `base::DoNothing` callback.
    // TODO(crbug.com/40248546): Future tests will need to know when the
    // deletion is completed, this will require a callback to be passed here.
    allowed_browsing_data_model()->RemoveBrowsingData(origin.host(),
                                                      base::DoNothing());
    allowed_browsing_data_model()->RemovePartitionedBrowsingData(
        origin.host(), net::SchemefulSite(origin), base::DoNothing());
    blocked_browsing_data_model()->RemoveBrowsingData(origin.host(),
                                                      base::DoNothing());
    blocked_browsing_data_model()->RemovePartitionedBrowsingData(
        origin.host(), net::SchemefulSite(origin), base::DoNothing());

    RecordPageSpecificSiteDataDialogRemoveButtonClickedAction();

    browsing_data::RecordDeleteBrowsingDataAction(
        browsing_data::DeleteBrowsingDataAction::kCookiesInUseDialog);
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
  }

  void OnManageOnDeviceSiteDataClicked() {
    Browser* browser = chrome::FindBrowserWithTab(web_contents_.get());
    chrome::ShowSettingsSubPage(browser, chrome::kOnDeviceSiteDataSubpage);
  }

  void OnRelatedApplicationLinkToAppSettings(const webapps::AppId& app_id) {
    web_app::OpenAppSettingsForInstalledRelatedApp(
        app_id,
        Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  }

 private:
  bool CanCreateContentException(GURL url) const { return !url.SchemeIsFile(); }

  PageSpecificSiteDataDialogSite CreateSiteFromEntryView(
      const BrowsingDataModel::BrowsingDataEntryView& entry,
      bool from_allowed_model) {
    // TODO(crbug.com/40205603): BDM provides host name only while
    // CookieTreeModel provides url::Origin. This classes works with
    // url::Origin, so here we convert host name to origin with some assumptions
    // (which might not be true). We should either convert to work only with
    // host names or BDM should return origins.
    url::Origin entry_origin = absl::visit(
        base::Overloaded{[&](const std::string& host) {
                           GURL current_url = web_contents_->GetVisibleURL();
                           GURL site_url = net::cookie_util::CookieOriginToURL(
                               host, current_url.SchemeIsCryptographic());
                           return url::Origin::Create(site_url);
                         },
                         [](const url::Origin& origin) { return origin; }},
        *entry.data_owner);
    return CreateSite(entry_origin, from_allowed_model,
                      IsBrowsingDataEntryViewFullyPartitioned(entry) &&
                          IsOnlyPartitionedStorageAccessAllowed(entry_origin));
  }

  bool IsBrowsingDataEntryViewFullyPartitioned(
      const BrowsingDataModel::BrowsingDataEntryView& entry) {
    return entry.GetThirdPartyPartitioningSite().has_value();
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
    // TODO(crbug.com/40231917): Handle sources other than SettingSource::kUser.
    return site;
  }

  bool IsOnlyPartitionedStorageAccessAllowed(url::Origin site_origin) {
    GURL current_url = web_contents_->GetVisibleURL();

    const bool block_third_party_cookies =
        cookie_settings_->ShouldBlockThirdPartyCookies();
    const auto default_content_setting =
        cookie_settings_->GetDefaultCookieSetting();
    ContentSetting first_party_setting =
        host_content_settings_map_->GetContentSetting(
            current_url, GURL(), ContentSettingsType::COOKIES);

    // Check for either a COOKIES or TRACKING_PROTECTION site exception.
    content_settings::SettingInfo info;
    host_content_settings_map_->GetContentSetting(
        site_origin.GetURL(), current_url, ContentSettingsType::COOKIES, &info);
    bool has_site_level_exception =
        info.primary_pattern != ContentSettingsPattern::Wildcard() ||
        info.secondary_pattern != ContentSettingsPattern::Wildcard();

    if (base::FeatureList::IsEnabled(
            privacy_sandbox::kTrackingProtectionContentSettingFor3pcb)) {
      has_site_level_exception |=
          tracking_protection_settings_->GetTrackingProtectionSetting(
              current_url) == CONTENT_SETTING_ALLOW;
    }

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
  // Each model represent separate data container. The implementation
  // doesn't make a difference between allowed and blocked models and checks
  // the actual content settings to determine the state.
  raw_ptr<BrowsingDataModel, DanglingUntriaged>
      allowed_browsing_data_model_for_testing_ = nullptr;
  raw_ptr<BrowsingDataModel, DanglingUntriaged>
      blocked_browsing_data_model_for_testing_ = nullptr;
  std::unique_ptr<FaviconCache> favicon_cache_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
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
        base::ranges::none_of(children(), [=, this](views::View* v) {
          return v != empty_state_label_ && v->GetVisible();
        });
    empty_state_label_->SetVisible(none_children_visible);
  }

  raw_ptr<views::Label> empty_state_label_ = nullptr;
};

std::unique_ptr<views::BoxLayoutView> CreateRelatedAppsView(
    Profile* profile,
    std::vector<webapps::AppId> all_controlling_apps,
    PageSpecificSiteDataDialogModelDelegate* delegate) {
  auto box_view = std::make_unique<views::BoxLayoutView>();
  box_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  box_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  for (const webapps::AppId& app_id : all_controlling_apps) {
    box_view->AddChildView(std::make_unique<RelatedAppRowView>(
        profile, app_id,
        // It is safe to use base::Unretained for the delegate here because both
        // the RelatedAppRowView and the delegate are owned by the dialog and
        // will be destroyed when the dialog is destroyed.
        base::BindRepeating(&PageSpecificSiteDataDialogModelDelegate::
                                OnRelatedApplicationLinkToAppSettings,
                            base::Unretained(delegate))));
  }

  return box_view;
}

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

std::vector<webapps::AppId>
PageSpecificSiteDataDialogTestApi::GetInstalledRelatedApps() {
  return delegate_->GetInstalledRelatedApps();
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
DEFINE_ELEMENT_IDENTIFIER_VALUE(kPageSpecificSiteDataDialogRelatedAppsSection);

// static
views::Widget* ShowPageSpecificSiteDataDialog(
    content::WebContents* web_contents) {
  auto delegate_unique =
      std::make_unique<PageSpecificSiteDataDialogModelDelegate>(web_contents);
  PageSpecificSiteDataDialogModelDelegate* delegate = delegate_unique.get();

  // Text replacement for on-device site data subtitle text which has an
  // embedded link to on-device site data settings page.
  ui::DialogModelLabel::TextReplacement settings_link =
      ui::DialogModelLabel::CreateLink(
          IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_SETTINGS_LINK,
          base::BindRepeating(&PageSpecificSiteDataDialogModelDelegate::
                                  OnManageOnDeviceSiteDataClicked,
                              base::Unretained(delegate)));
  auto builder = ui::DialogModel::Builder(std::move(delegate_unique));
  builder
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_TITLE))
      .AddParagraph(
          ui::DialogModelLabel::CreateWithReplacement(
              IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_SUBTITLE, settings_link)
              .set_is_secondary())
      .SetInternalName("PageSpecificSiteDataDialog")
      .AddOkButton(
          base::BindRepeating(&PageSpecificSiteDataDialogModelDelegate::
                                  OnDialogExplicitlyClosed,
                              base::Unretained(delegate)),
          ui::DialogModel::Button::Params().SetLabel(
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
    if (section.sites.size() == 0u) {
      continue;
    }

    has_any_sections = true;
    builder.AddParagraph(
        ui::DialogModelLabel(section.subtitle).set_is_secondary(),
        section.title);
    builder.AddCustomField(
        CreateCustomField(std::make_unique<PageSpecificSiteDataSectionView>(
            profile, section.sites, delegate)),
        section.identifier);
  }

  // Decide whether to show any apps installed from a related site origin.
  if (base::FeatureList::IsEnabled(
          features::kPageSpecificDataDialogRelatedInstalledAppsSection)) {
    std::vector<webapps::AppId> all_controlling_apps_list =
        delegate->GetInstalledRelatedApps();

    if (!all_controlling_apps_list.empty()) {
      has_any_sections = true;

      builder.AddParagraph(
          ui::DialogModelLabel(
              l10n_util::GetStringUTF16(
                  IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_RELATED_APPS_SUBTITLE))
              .set_is_secondary(),
          l10n_util::GetStringUTF16(
              IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_RELATED_APPS_TITLE));

      builder.AddCustomField(
          CreateCustomField(CreateRelatedAppsView(
              profile, std::move(all_controlling_apps_list), delegate)),
          kPageSpecificSiteDataDialogRelatedAppsSection);
    }
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
