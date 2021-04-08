// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MAIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MAIN_VIEW_H_

#include "build/build_config.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view_observer.h"
#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row_observer.h"
#include "chrome/browser/ui/views/page_info/security_information_view.h"
#include "components/page_info/page_info_ui.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/view.h"

class Profile;

// The main view of the page info, contains security information, permissions
// and  site-related settings. This is used in the experimental
// PageInfoNewBubbleView (under a flag PageInfoV2Desktop).
class PageInfoMainView : public views::View,
                         public PageInfoUI,
                         public PermissionSelectorRowObserver,
                         public ChosenObjectViewObserver {
 public:
  // The width of the column size for permissions and chosen object icons.
  static constexpr int kIconColumnWidth = 16;
  // The column set id of the permissions table for |permissions_view_|.
  static constexpr int kPermissionColumnSetId = 0;

  PageInfoMainView(PageInfo* presenter,
                   PageInfoUiDelegate* ui_delegate,
                   Profile* profile);
  ~PageInfoMainView() override;

  enum PageInfoBubbleViewID {
    VIEW_ID_NONE = 0,
    VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD,
    VIEW_ID_PAGE_INFO_BUTTON_ALLOWLIST_PASSWORD_REUSE,
    VIEW_ID_PAGE_INFO_LABEL_EV_CERTIFICATE_DETAILS,
    VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG,
    VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS,
    VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER,
    VIEW_ID_PAGE_INFO_BUTTON_END_VR,
    VIEW_ID_PAGE_INFO_HOVER_BUTTON_VR_PRESENTATION,
    VIEW_ID_PAGE_INFO_BUTTON_LEAVE_SITE,
    VIEW_ID_PAGE_INFO_BUTTON_IGNORE_WARNING,
  };

  // PageInfoUI implementations.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;
  void SetPageFeatureInfo(const PageFeatureInfo& info) override;

  void LayoutPermissionsLikeUiRow(views::GridLayout* layout,
                                  bool is_list_empty,
                                  int column_id);

  gfx::Size CalculatePreferredSize() const override;

  // PermissionSelectorRowObserver:
  void OnPermissionChanged(const PageInfo::PermissionInfo& permission) override;

  // ChosenObjectViewObserver:
  void OnChosenObjectDeleted(const PageInfoUI::ChosenObjectInfo& info) override;

 protected:
  // TODO(olesiamarukhno): Was used for tests, will update it after redesigning
  // moves forward.
  const std::u16string details_text() const { return details_text_; }

 private:
  // Creates the contents of the |site_settings_view_|.
  std::unique_ptr<views::View> CreateSiteSettingsView() WARN_UNUSED_RESULT;

  // Creates bubble header view for this page, contains the title and the close
  // button.
  std::unique_ptr<views::View> CreateBubbleHeaderView() WARN_UNUSED_RESULT;

  // Posts a task to HandleMoreInfoRequestAsync() below.
  void HandleMoreInfoRequest(views::View* source);

  // Used to asynchronously handle clicks since these calls may cause the
  // destruction of the settings view and the base class window still needs to
  // be alive to finish handling the mouse or keyboard click.
  void HandleMoreInfoRequestAsync(int view_id);

  void ResetDecisionsClicked();

  void SecurityDetailsClicked(const ui::Event& event);

  PageInfoUI::SecurityDescriptionType GetSecurityDescriptionType() const;

  void SetSecurityDescriptionType(
      const PageInfoUI::SecurityDescriptionType& type);

  PageInfo* presenter_;

  PageInfoUiDelegate* ui_delegate_;

  // The raw details of the status of the identity check for this site.
  std::u16string details_text_ = std::u16string();

  // The view that contains the certificate, cookie, and permissions sections.
  views::View* site_settings_view_ = nullptr;

  // The button that opens the "Cookies" dialog.
  PageInfoHoverButton* cookie_button_ = nullptr;

  // The button that opens the "Certificate" dialog.
  PageInfoHoverButton* certificate_button_ = nullptr;

  // The button that opens up "Site Settings".
  views::View* site_settings_link = nullptr;

  // The view that contains the "Permissions" table of the bubble.
  views::View* permissions_view_ = nullptr;

#if defined(OS_WIN) && BUILDFLAG(ENABLE_VR)
  // The view that contains ui related to features on a page, like a presenting
  // VR page.
  views::View* page_feature_info_view_ = nullptr;
#endif

  // The certificate provided by the site, if one exists.
  scoped_refptr<net::X509Certificate> certificate_;

  // These rows bundle together all the |View|s involved in a single row of the
  // permissions section, and keep those views updated when the underlying
  // |Permission| changes.
  std::vector<std::unique_ptr<PermissionSelectorRow>> selector_rows_;

  views::Label* title_ = nullptr;

  SecurityInformationView* security_view_ = nullptr;

  // TODO(olesiamarukhno): Was used for tests, will update it after redesigning
  // moves forward.
  PageInfoUI::SecurityDescriptionType security_description_type_ =
      PageInfoUI::SecurityDescriptionType::CONNECTION;

  base::WeakPtrFactory<PageInfoMainView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_MAIN_VIEW_H_
