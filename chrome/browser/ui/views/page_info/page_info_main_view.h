// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MAIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MAIN_VIEW_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view_observer.h"
#include "chrome/browser/ui/views/page_info/permission_toggle_row_view_observer.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/page_info/core/page_info_types.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/page_info/page_info_ui.h"
#include "device/vr/buildflags/buildflags.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
class LabelButton;
}  // namespace views

class ChromePageInfoUiDelegate;
class ChosenObjectView;
class RichHoverButton;
class PageInfoNavigationHandler;
class PageInfoSecurityContentView;
class PermissionToggleRowView;

namespace test {
class PageInfoBubbleViewTestApi;
}  // namespace test

// The main view of the page info, contains security information, permissions
// and  site-related settings.
class PageInfoMainView : public views::View,
                         public PageInfoUI,
                         public PermissionToggleRowViewObserver,
                         public ChosenObjectViewObserver {
  METADATA_HEADER(PageInfoMainView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCookieButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPrivacyAndSiteDataButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMainLayoutElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPermissionsElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMerchantTrustElementId);

  // Container view that fills the bubble width for button rows. Supports
  // updating the layout.
  class ContainerView : public views::View {
    METADATA_HEADER(ContainerView, views::View)

   public:
    explicit ContainerView(bool set_extra_right_margin = true);

    // Notifies that preferred size changed and updates the layout.
    void Update();
  };

  PageInfoMainView(PageInfo* presenter,
                   ChromePageInfoUiDelegate* ui_delegate,
                   PageInfoNavigationHandler* navigation_handler,
                   base::OnceClosure initialized_callback,
                   bool allow_extended_site_info);
  ~PageInfoMainView() override;

  // PageInfoUI implementations.
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;
  void SetPageFeatureInfo(const PageFeatureInfo& info) override;
  void SetAdPersonalizationInfo(const AdPersonalizationInfo& info) override;
  void SetCookieInfo(const CookiesInfo& cookie_info) override;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // PermissionToggleRowViewObserver:
  void OnPermissionChanged(const PageInfo::PermissionInfo& permission) override;

  // ChosenObjectViewObserver:
  void OnChosenObjectDeleted(const PageInfoUI::ChosenObjectInfo& info) override;

  int GetVisiblePermissionsCountForTesting() const {
    return toggle_rows_.size();
  }

  const std::vector<raw_ptr<PermissionToggleRowView, VectorExperimental>>&
  GetToggleRowsForTesting() const {
    return toggle_rows_;
  }

 protected:
  // TODO(olesiamarukhno): Was used for tests, will update it after redesigning
  // moves forward.
  const std::u16string details_text() const { return details_text_; }

 private:
  friend class PageInfoBubbleViewDialogBrowserTest;
  friend class test::PageInfoBubbleViewTestApi;

  // Creates a view with vertical box layout that will used a container for
  // other views.
  [[nodiscard]] std::unique_ptr<views::View> CreateContainerView(
      bool set_extra_right_margin = true);

  // Creates bubble header view for this page, contains the title and the close
  // button.
  [[nodiscard]] std::unique_ptr<views::View> CreateBubbleHeaderView();

  // Posts a task to HandleMoreInfoRequestAsync() below.
  void HandleMoreInfoRequest(views::View* source);

  // Used to asynchronously handle clicks since these calls may cause the
  // destruction of the settings view and the base class window still needs to
  // be alive to finish handling the mouse or keyboard click.
  void HandleMoreInfoRequestAsync(int view_id);

  // Makes the permission reset button visible if there is any permission and
  // enables it if any permission is in a non-default state. Also updates
  // the label depending on the number of visible permissions.
  void UpdateResetButton(const PermissionInfoList& permission_info_list);

  void OnMerchantTrustDataFetched(
      const GURL& url,
      std::optional<page_info::MerchantData> merchant_data);

  // Creates 'About this site' button that opens a subpage.
  [[nodiscard]] std::unique_ptr<views::View> CreateAboutThisSiteButton(
      const page_info::proto::SiteInfo& info);

  // Creates 'Ad personalization' button that opens a subpage.
  [[nodiscard]] std::unique_ptr<views::View> CreateAdPersonalizationButton();

  // Creates 'Merchant trust' button.
  [[nodiscard]] std::unique_ptr<views::View> CreateMerchantTrustButton(
      page_info::MerchantData value);

  // Creates 'Merchant trust' button that opens a subpage.
  [[nodiscard]] std::unique_ptr<RichHoverButton>
  CreateMerchantTrustSubpageButton(page_info::MerchantData value);

  // Creates 'Merchant trust' button that opens a side panel.
  [[nodiscard]] std::unique_ptr<RichHoverButton>
  CreateMerchantTrustLaunchButton(GURL url);

  void OpenMerchantTrustSidePanel(const GURL& url);

  raw_ptr<PageInfo, AcrossTasksDanglingUntriaged> presenter_;

  raw_ptr<ChromePageInfoUiDelegate, AcrossTasksDanglingUntriaged> ui_delegate_;

  raw_ptr<PageInfoNavigationHandler> navigation_handler_;

  // The raw details of the status of the identity check for this site.
  std::u16string details_text_ = std::u16string();

  // The button that opens the "Connection" subpage.
  raw_ptr<RichHoverButton, AcrossTasksDanglingUntriaged> connection_button_ =
      nullptr;

  // The view that contains the certificate, cookie, and permissions sections.
  raw_ptr<views::View> site_settings_view_ = nullptr;

  // The button that opens the "Cookies" subpage.
  raw_ptr<RichHoverButton> cookie_button_ = nullptr;

  // The button that opens up "Site Settings".
  raw_ptr<RichHoverButton> site_settings_link_ = nullptr;

  // The view that contains the scroll view with permission rows and the reset
  // button, surrounded by separators.
  raw_ptr<views::View> permissions_view_ = nullptr;

  // The section that contains a separator and buttons with extended site
  // information: "About this site" and "Merchant trust". The section is only
  // shown when at least of one the buttons is available.
  raw_ptr<views::View> extended_site_info_section_ = nullptr;

  // "About this site" button that opens a subpage.
  raw_ptr<views::View> about_this_site_section_ = nullptr;

  // "Merchant trust" button that opens a subpage.
  raw_ptr<views::View> merchant_trust_section_ = nullptr;

  // The view that contains `SecurityInformationView` and a certificate button.
  raw_ptr<PageInfoSecurityContentView, AcrossTasksDanglingUntriaged>
      security_content_view_ = nullptr;

  // "Ad personalization" button that opens a subpage.
  raw_ptr<views::View> ads_personalization_section_ = nullptr;

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_VR)
  // The view that contains ui related to features on a page, like a presenting
  // VR page.
  raw_ptr<views::View> page_feature_info_view_ = nullptr;
#endif

  // These rows bundle together all the |View|s involved in a single row of the
  // permissions section, and keep those views updated when the underlying
  // |Permission| changes.
  std::vector<raw_ptr<PermissionToggleRowView, VectorExperimental>>
      toggle_rows_;

  // This map contains rows for permission types that should be updated when
  // their usage status is changed. This is needed to avoid recreating the whole
  // PageInfo view if only a single permission is changed.
  std::map<ContentSettingsType, raw_ptr<PermissionToggleRowView>>
      syncable_permission_rows_;

  std::vector<raw_ptr<ChosenObjectView, VectorExperimental>>
      chosen_object_rows_;

  raw_ptr<views::Label> title_ = nullptr;

  raw_ptr<views::View> security_container_view_ = nullptr;

  raw_ptr<views::LabelButton, AcrossTasksDanglingUntriaged> reset_button_ =
      nullptr;

  base::WeakPtrFactory<PageInfoMainView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MAIN_VIEW_H_
