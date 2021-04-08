// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/reputation/safety_tip_ui.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view_observer.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row_observer.h"
#include "chrome/browser/ui/views/page_info/security_information_view.h"
#include "components/page_info/page_info_ui.h"
#include "components/safe_browsing/buildflags.h"
#include "components/security_state/core/security_state.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/widget/widget.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace net {
class X509Certificate;
}  // namespace net

namespace test {
class PageInfoBubbleViewTestApi;
}  // namespace test

namespace views {
class View;
}  // namespace views

// The views implementation of the page info UI.
// TODO(crbug.com/1188101): This will be deprecated in favor of
// PageInfoNewBubbleView (which is under PageInfoV2Desktop) when it is finished.
class PageInfoBubbleView : public PageInfoBubbleViewBase,
                           public PermissionSelectorRowObserver,
                           public ChosenObjectViewObserver,
                           public PageInfoUI {
 public:
  METADATA_HEADER(PageInfoBubbleView);

  // The width of the column size for permissions and chosen object icons.
  static constexpr int kIconColumnWidth = 16;
  // The column set id of the permissions table for |permissions_view_|.
  static constexpr int kPermissionColumnSetId = 0;

  PageInfoBubbleView(const PageInfoBubbleView&) = delete;
  PageInfoBubbleView& operator=(const PageInfoBubbleView&) = delete;
  ~PageInfoBubbleView() override;

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

  // Creates the appropriate page info bubble for the given |url|.
  // |anchor_view| will be used to place the bubble.  If |anchor_view| is null,
  // |anchor_rect| will be used instead.  |parent_window| will become the
  // parent of the widget hosting the bubble view.
  static views::BubbleDialogDelegateView* CreatePageInfoBubble(
      views::View* anchor_view,
      const gfx::Rect& anchor_rect,
      gfx::NativeWindow parent_window,
      Profile* profile,
      content::WebContents* web_contents,
      const GURL& url,
      PageInfoClosingCallback closing_callback);

  void SecurityDetailsClicked(const ui::Event& event);
  void ResetDecisionsClicked();

  PageInfoUI::SecurityDescriptionType GetSecurityDescriptionType() const;
  void SetSecurityDescriptionType(
      const PageInfoUI::SecurityDescriptionType& type);

 protected:
  const std::u16string details_text() const { return details_text_; }

 private:
  friend class PageInfoBubbleViewBrowserTest;
  friend class PageInfoBubbleViewSyncBrowserTest;
  friend class test::PageInfoBubbleViewTestApi;

  PageInfoBubbleView(
      views::View* anchor_view,
      const gfx::Rect& anchor_rect,
      gfx::NativeView parent_window,
      Profile* profile,
      content::WebContents* web_contents,
      const GURL& url,
      PageInfoClosingCallback closing_callback);

  // PageInfoBubbleViewBase:
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void WebContentsDestroyed() override;

  // PermissionSelectorRowObserver:
  void OnPermissionChanged(const PageInfo::PermissionInfo& permission) override;

  // ChosenObjectViewObserver:
  void OnChosenObjectDeleted(const PageInfoUI::ChosenObjectInfo& info) override;

  // PageInfoUI:
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;
  void SetPageFeatureInfo(const PageFeatureInfo& info) override;

  void LayoutPermissionsLikeUiRow(views::GridLayout* layout,
                                  bool is_list_empty,
                                  int column_id);

  // WebContentsObserver:
  void DidChangeVisibleSecurityState() override;

  // Creates the contents of the |site_settings_view_|.
  std::unique_ptr<views::View> CreateSiteSettingsView() WARN_UNUSED_RESULT;

  // Posts a task to HandleMoreInfoRequestAsync() below.
  void HandleMoreInfoRequest(views::View* source);

  // Used to asynchronously handle clicks since these calls may cause the
  // destruction of the settings view and the base class window still needs to
  // be alive to finish handling the mouse or keyboard click.
  void HandleMoreInfoRequestAsync(int view_id);

  // The presenter that controls the Page Info UI.
  std::unique_ptr<PageInfo> presenter_;

  Profile* const profile_;

  // The header section (containing security-related information).
  SecurityInformationView* header_ = nullptr;

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

  // The view that contains ui related to features on a page, like a presenting
  // VR page.
  views::View* page_feature_info_view_ = nullptr;

  // The certificate provided by the site, if one exists.
  scoped_refptr<net::X509Certificate> certificate_;

  // These rows bundle together all the |View|s involved in a single row of the
  // permissions section, and keep those views updated when the underlying
  // |Permission| changes.
  std::vector<std::unique_ptr<PermissionSelectorRow>> selector_rows_;

  PageInfoClosingCallback closing_callback_;

  PageInfoUI::SecurityDescriptionType security_description_type_ =
      PageInfoUI::SecurityDescriptionType::CONNECTION;

  std::unique_ptr<PageInfoUiDelegate> ui_delegate_;

  base::WeakPtrFactory<PageInfoBubbleView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_H_
