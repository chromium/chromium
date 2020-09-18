// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reputation/safety_tip_ui_helper.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view.h"
#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/page_info/page_info.h"
#include "components/safe_browsing/buildflags.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#endif

using bubble_anchor_util::AnchorConfiguration;
using bubble_anchor_util::GetPageInfoAnchorConfiguration;
using bubble_anchor_util::GetPageInfoAnchorRect;

namespace {

// General constants -----------------------------------------------------------

// Bubble width constraints.
constexpr int kMinBubbleWidth = 320;
constexpr int kMaxBubbleWidth = 1000;

SkColor GetRelatedTextColor() {
  views::Label label;
  return views::style::GetColor(label, views::style::CONTEXT_LABEL,
                                views::style::STYLE_PRIMARY);
}

// Adds a ColumnSet on |layout| with a single View column and padding columns
// on either side of it with |margin| width.
void AddColumnWithSideMargin(views::GridLayout* layout, int margin, int id) {
  views::ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, margin);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, margin);
}

std::unique_ptr<views::View> CreateSiteSettingsLink(
    const int side_margin,
    views::ButtonListener* listener) {
  const base::string16& tooltip =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_SETTINGS_TOOLTIP);
  return std::make_unique<PageInfoHoverButton>(
      listener, PageInfoUI::GetSiteSettingsIcon(GetRelatedTextColor()),
      IDS_PAGE_INFO_SITE_SETTINGS_LINK, base::string16(),
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS,
      tooltip, base::string16());
}

}  // namespace

// BubbleHeaderView is the UI element (view) that represents the header of a
// PageInfoBubbleView. The header shows the status of the site's identity check
// and the name of the site's identity.
class BubbleHeaderView : public views::View {
 public:
  BubbleHeaderView(PageInfoBubbleView* bubble, int side_margin);
  ~BubbleHeaderView() override;

  // Sets the security summary for the current page.
  void SetSummary(const base::string16& summary_text);

  // Sets the security details for the current page.
  void SetDetails(const base::string16& details_text);

  void AddEvCertificateDetailsLabel(
      const PageInfoBubbleView::IdentityInfo& identity_info);

  void AddResetDecisionsLabel();

  // Adds the change password and mark site as legitimate buttons.
  // If |is_saved_password|, adds a check password button instead of
  // change password button.
  void AddPasswordReuseButtons(bool is_saved_password);

 private:
  // Owns |this|.
  PageInfoBubbleView* bubble_;

  // The label that displays the status of the identity check for this site.
  // Includes a link to open the Chrome Help Center article about connection
  // security.
  views::StyledLabel* security_details_label_ = nullptr;

  // A container for the styled label containing organization name and
  // jurisdiction details, if the site has an EV certificate.
  // This is only shown sometimes, so we use a container to keep track of where
  // to place it (if needed).
  views::View* ev_certificate_label_container_ = nullptr;

  // A container for the styled label with a link for resetting cert decisions.
  // This is only shown sometimes, so we use a container to keep track of
  // where to place it (if needed).
  views::View* reset_decisions_label_container_ = nullptr;

  // A container for the label buttons used to change password or mark the site
  // as safe.
  views::View* password_reuse_button_container_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BubbleHeaderView);
};

// The regular PageInfoBubbleView is not supported for internal Chrome pages and
// extension pages. Instead of the |PageInfoBubbleView|, the
// |InternalPageInfoBubbleView| is displayed.
class InternalPageInfoBubbleView : public PageInfoBubbleViewBase {
 public:
  // If |anchor_view| is nullptr, or has no Widget, |parent_window| may be
  // provided to ensure this bubble is closed when the parent closes.
  InternalPageInfoBubbleView(views::View* anchor_view,
                             const gfx::Rect& anchor_rect,
                             gfx::NativeView parent_window,
                             content::WebContents* web_contents,
                             const GURL& url);
  ~InternalPageInfoBubbleView() override;

  gfx::Size CalculatePreferredSize() const override;

  DISALLOW_COPY_AND_ASSIGN(InternalPageInfoBubbleView);
};

////////////////////////////////////////////////////////////////////////////////
// Bubble Header
////////////////////////////////////////////////////////////////////////////////

BubbleHeaderView::BubbleHeaderView(PageInfoBubbleView* bubble, int side_margin)
    : bubble_(bubble) {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  const int label_column_status = 1;
  AddColumnWithSideMargin(layout, side_margin, label_column_status);

  layout->StartRow(views::GridLayout::kFixedSize, label_column_status);

  auto security_details_label = std::make_unique<views::StyledLabel>();
  security_details_label_ =
      layout->AddView(std::move(security_details_label), 1.0, 1.0,
                      views::GridLayout::FILL, views::GridLayout::LEADING);

  layout->StartRow(views::GridLayout::kFixedSize, label_column_status);
  auto reset_decisions_label_container = std::make_unique<views::View>();
  reset_decisions_label_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  reset_decisions_label_container_ =
      layout->AddView(std::move(reset_decisions_label_container), 1.0, 1.0,
                      views::GridLayout::FILL, views::GridLayout::LEADING);

  layout->StartRow(views::GridLayout::kFixedSize, label_column_status);
  password_reuse_button_container_ =
      layout->AddView(std::make_unique<views::View>(), 1, 1,
                      views::GridLayout::FILL, views::GridLayout::LEADING);
}

BubbleHeaderView::~BubbleHeaderView() = default;

void BubbleHeaderView::SetDetails(const base::string16& details_text) {
  std::vector<base::string16> subst;
  subst.push_back(details_text);
  subst.push_back(l10n_util::GetStringUTF16(IDS_LEARN_MORE));

  std::vector<size_t> offsets;

  base::string16 text = base::ReplaceStringPlaceholders(
      base::ASCIIToUTF16("$1 $2"), subst, &offsets);
  security_details_label_->SetText(text);
  gfx::Range details_range(offsets[1], text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&PageInfoBubbleView::SecurityDetailsClicked,
                              base::Unretained(bubble_)));
  link_style.disable_line_wrapping = false;

  security_details_label_->AddStyleRange(details_range, link_style);
}

void BubbleHeaderView::AddResetDecisionsLabel() {
  if (!reset_decisions_label_container_->children().empty()) {
    // Ensure all old content is removed from the container before re-adding it.
    reset_decisions_label_container_->RemoveAllChildViews(true);
  }

  std::vector<base::string16> subst;
  subst.push_back(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_INVALID_CERTIFICATE_DESCRIPTION));
  subst.push_back(l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_RESET_INVALID_CERTIFICATE_DECISIONS_BUTTON));

  std::vector<size_t> offsets;

  base::string16 text = base::ReplaceStringPlaceholders(
      base::ASCIIToUTF16("$1 $2"), subst, &offsets);
  views::StyledLabel* reset_cert_decisions_label =
      reset_decisions_label_container_->AddChildView(
          std::make_unique<views::StyledLabel>());
  reset_cert_decisions_label->SetText(text);
  gfx::Range link_range(offsets[1], text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&PageInfoBubbleView::ResetDecisionsClicked,
                              base::Unretained(bubble_)));
  link_style.disable_line_wrapping = false;

  reset_cert_decisions_label->AddStyleRange(link_range, link_style);
  // Fit the styled label to occupy available width.
  reset_cert_decisions_label->SizeToFit(0);

  // Now that it contains a label, the container needs padding at the top.
  reset_decisions_label_container_->SetBorder(views::CreateEmptyBorder(
      8, views::GridLayout::kFixedSize, views::GridLayout::kFixedSize, 0));

  InvalidateLayout();
}

void BubbleHeaderView::AddPasswordReuseButtons(bool is_saved_password) {
  if (!password_reuse_button_container_->children().empty()) {
    // Ensure all old content is removed from the container before re-adding it.
    password_reuse_button_container_->RemoveAllChildViews(true /* delete */);
  }

  int change_password_template = is_saved_password
                                     ? IDS_PAGE_INFO_CHECK_PASSWORDS_BUTTON
                                     : IDS_PAGE_INFO_CHANGE_PASSWORD_BUTTON;

  std::unique_ptr<views::MdTextButton> change_password_button;
  if (change_password_template) {
    change_password_button = std::make_unique<views::MdTextButton>(
        bubble_, l10n_util::GetStringUTF16(change_password_template));
    change_password_button->SetProminent(true);
    change_password_button->SetID(
        PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  }
  auto allowlist_password_reuse_button = std::make_unique<views::MdTextButton>(
      bubble_,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_ALLOWLIST_PASSWORD_REUSE_BUTTON));
  allowlist_password_reuse_button->SetID(
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_ALLOWLIST_PASSWORD_REUSE);

  int kSpacingBetweenButtons = 8;
  int change_password_button_size =
      change_password_button
          ? change_password_button->CalculatePreferredSize().width()
          : 0;

  // If these two buttons cannot fit into a single line, stack them vertically.
  bool can_fit_in_one_line =
      (password_reuse_button_container_->width() - kSpacingBetweenButtons) >=
      (change_password_button_size +
       allowlist_password_reuse_button->CalculatePreferredSize().width());
  auto layout = std::make_unique<views::BoxLayout>(
      can_fit_in_one_line ? views::BoxLayout::Orientation::kHorizontal
                          : views::BoxLayout::Orientation::kVertical,
      gfx::Insets(), kSpacingBetweenButtons);
  // Make buttons left-aligned. For RTL languages, buttons will automatically
  // become right-aligned.
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  password_reuse_button_container_->SetLayoutManager(std::move(layout));

#if defined(OS_WIN) || defined(OS_CHROMEOS)
  if (change_password_button) {
    password_reuse_button_container_->AddChildView(
        std::move(change_password_button));
  }
  password_reuse_button_container_->AddChildView(
      std::move(allowlist_password_reuse_button));
#else
  password_reuse_button_container_->AddChildView(
      std::move(allowlist_password_reuse_button));
  if (change_password_button) {
    password_reuse_button_container_->AddChildView(
        std::move(change_password_button));
  }
#endif

  // Add padding at the top.
  password_reuse_button_container_->SetBorder(
      views::CreateEmptyBorder(8, views::GridLayout::kFixedSize, 0, 0));

  InvalidateLayout();
}

////////////////////////////////////////////////////////////////////////////////
// InternalPageInfoBubbleView
////////////////////////////////////////////////////////////////////////////////

InternalPageInfoBubbleView::InternalPageInfoBubbleView(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    content::WebContents* web_contents,
    const GURL& url)
    : PageInfoBubbleViewBase(anchor_view,
                             anchor_rect,
                             parent_window,
                             PageInfoBubbleViewBase::BUBBLE_INTERNAL_PAGE,
                             web_contents) {
  int text = IDS_PAGE_INFO_INTERNAL_PAGE;
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    text = IDS_PAGE_INFO_EXTENSION_PAGE;
  } else if (url.SchemeIs(content::kViewSourceScheme)) {
    text = IDS_PAGE_INFO_VIEW_SOURCE_PAGE;
  } else if (url.SchemeIs(url::kFileScheme)) {
    text = IDS_PAGE_INFO_FILE_PAGE;
  } else if (url.SchemeIs(content::kChromeDevToolsScheme)) {
    text = IDS_PAGE_INFO_DEVTOOLS_PAGE;
  } else if (url.SchemeIs(dom_distiller::kDomDistillerScheme)) {
    if (dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(url).SchemeIs(
            url::kHttpsScheme)) {
      text = IDS_PAGE_INFO_READER_MODE_PAGE_SECURE;
    } else {
      text = IDS_PAGE_INFO_READER_MODE_PAGE;
    }
  } else if (!url.SchemeIs(content::kChromeUIScheme)) {
    NOTREACHED();
  }

  // Title insets assume there is content (and thus have no bottom padding). Use
  // dialog insets to get the bottom margin back.
  set_title_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  set_margins(gfx::Insets());

  SetTitle(text);

  views::BubbleDialogDelegateView::CreateBubble(this);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  // Use a normal label's style for the title since there is no content.
  views::Label* title_label =
      static_cast<views::Label*>(GetBubbleFrameView()->title());
  title_label->SetFontList(views::Label::GetDefaultFontList());
  title_label->SetMultiLine(true);
  title_label->SetElideBehavior(gfx::NO_ELIDE);

  SizeToContents();
}

InternalPageInfoBubbleView::~InternalPageInfoBubbleView() {}

gfx::Size InternalPageInfoBubbleView::CalculatePreferredSize() const {
  // Without a layout manager this will recurse infinitely
  // (GetHeightForWidth() calls CalculatePreferredSize()).
  // TODO(crbug.com/1128500): Fix infinite recursion or always
  // install a layout manager.
  if (!GetLayoutManager())
    return gfx::Size();

  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

////////////////////////////////////////////////////////////////////////////////
// PageInfoBubbleView
////////////////////////////////////////////////////////////////////////////////

PageInfoBubbleView::~PageInfoBubbleView() {}

// static
views::BubbleDialogDelegateView* PageInfoBubbleView::CreatePageInfoBubble(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeWindow parent_window,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    PageInfoClosingCallback closing_callback) {
  gfx::NativeView parent_view = platform_util::GetViewForWindow(parent_window);

  if (PageInfo::IsFileOrInternalPage(url) ||
      url.SchemeIs(extensions::kExtensionScheme) ||
      url.SchemeIs(dom_distiller::kDomDistillerScheme)) {
    return new InternalPageInfoBubbleView(anchor_view, anchor_rect, parent_view,
                                          web_contents, url);
  }

  return new PageInfoBubbleView(anchor_view, anchor_rect, parent_view, profile,
                                web_contents, url, std::move(closing_callback));
}

void PageInfoBubbleView::SecurityDetailsClicked(const ui::Event& event) {
  if (GetSecurityDescriptionType() == SecurityDescriptionType::SAFETY_TIP) {
    OpenHelpCenterFromSafetyTip(web_contents());
  } else {
    web_contents()->OpenURL(content::OpenURLParams(
        GURL(chrome::kPageInfoHelpCenterURL), content::Referrer(),
        ui::DispositionFromEventFlags(
            event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB),
        ui::PAGE_TRANSITION_LINK, false));
    presenter_->RecordPageInfoAction(
        PageInfo::PAGE_INFO_CONNECTION_HELP_OPENED);
  }
}

void PageInfoBubbleView::ResetDecisionsClicked() {
  presenter_->OnRevokeSSLErrorBypassButtonPressed();
  GetWidget()->Close();
}

PageInfoBubbleView::PageInfoBubbleView(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    PageInfoClosingCallback closing_callback)
    : PageInfoBubbleViewBase(anchor_view,
                             anchor_rect,
                             parent_window,
                             PageInfoBubbleViewBase::BUBBLE_PAGE_INFO,
                             web_contents),
      profile_(profile),
      closing_callback_(std::move(closing_callback)) {
  DCHECK(closing_callback_);

  // Capture the default bubble margin, and move it to the Layout classes. This
  // is necessary so that the views::Separator can extend the full width of the
  // bubble.
  const int side_margin = margins().left();
  DCHECK_EQ(margins().left(), margins().right());

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // In Harmony, the last view is a HoverButton, which overrides the bottom
  // dialog inset in favor of its own. Note the multi-button value is used here
  // assuming that the "Cookies" & "Site settings" buttons will always be shown.
  const int hover_list_spacing =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  const int bottom_margin = hover_list_spacing;
  set_margins(gfx::Insets(margins().top(), 0, bottom_margin, 0));

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  constexpr int kColumnId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  header_ =
      layout->AddView(std::make_unique<BubbleHeaderView>(this, side_margin));

  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  permissions_view_ = layout->AddView(std::make_unique<views::View>());

  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  layout->AddView(std::make_unique<views::Separator>());

  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnId,
                              views::GridLayout::kFixedSize,
                              hover_list_spacing);
  site_settings_view_ = layout->AddView(CreateSiteSettingsView());

  if (!profile->IsGuestSession()) {
    layout->StartRowWithPadding(views::GridLayout::kFixedSize, kColumnId,
                                views::GridLayout::kFixedSize, 0);
    layout->AddView(CreateSiteSettingsLink(side_margin, this));
  }

#if defined(OS_WIN) && BUILDFLAG(ENABLE_VR)
  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  page_feature_info_view_ = layout->AddView(std::make_unique<views::View>());
#endif

  views::BubbleDialogDelegateView::CreateBubble(this);

  // CreateBubble() may not set our size synchronously so explicitly set it here
  // before PageInfo updates trigger child layouts.
  SetSize(GetPreferredSize());

  presenter_ = std::make_unique<PageInfo>(
      std::make_unique<ChromePageInfoDelegate>(web_contents), web_contents,
      url);
  presenter_->InitializeUiState(this);
}

void PageInfoBubbleView::WebContentsDestroyed() {
  weak_factory_.InvalidateWeakPtrs();
}

void PageInfoBubbleView::OnPermissionChanged(
    const PageInfo::PermissionInfo& permission) {
  presenter_->OnSitePermissionChanged(permission.type, permission.setting);
  // The menu buttons for the permissions might have longer strings now, so we
  // need to layout and size the whole bubble.
  Layout();
  SizeToContents();
}

void PageInfoBubbleView::OnChosenObjectDeleted(
    const PageInfoUI::ChosenObjectInfo& info) {
  presenter_->OnSiteChosenObjectDeleted(info.ui_info,
                                        info.chooser_object->value);
}

void PageInfoBubbleView::OnWidgetDestroying(views::Widget* widget) {
  PageInfoBubbleViewBase::OnWidgetDestroying(widget);

  bool reload_prompt;
  presenter_->OnUIClosing(&reload_prompt);

  // This method mostly shouldn't be re-entrant but there are a few cases where
  // it can be (see crbug/966308). In that case, we have already run the closing
  // callback so should not attempt to do it again.
  if (closing_callback_)
    std::move(closing_callback_).Run(widget->closed_reason(), reload_prompt);
}

void PageInfoBubbleView::ButtonPressed(views::Button* button,
                                       const ui::Event& event) {
  switch (button->GetID()) {
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_CLOSE:
      GetWidget()->Close();
      break;
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD:
      presenter_->OnChangePasswordButtonPressed(web_contents());
      break;
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_ALLOWLIST_PASSWORD_REUSE:
      GetWidget()->Close();
      presenter_->OnWhitelistPasswordReuseButtonPressed(web_contents());
      break;
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS:
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG:
    case PageInfoBubbleView::
        VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER:
      HandleMoreInfoRequest(button);
      break;
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_HOVER_BUTTON_VR_PRESENTATION:
      // Ignore clicks on the "VR is presenting" row.
      break;
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_END_VR:
      GetWidget()->Close();
#if BUILDFLAG(ENABLE_VR)
      vr::VrTabHelper::ExitVrPresentation();
#endif
      break;
    default:
      NOTREACHED();
  }
}

gfx::Size PageInfoBubbleView::CalculatePreferredSize() const {
  if (header_ == nullptr && site_settings_view_ == nullptr) {
    return views::View::CalculatePreferredSize();
  }

  int width = kMinBubbleWidth;
  if (site_settings_view_) {
    width = std::max(width, permissions_view_->GetPreferredSize().width());
    width = std::min(width, kMaxBubbleWidth);
  }
  return gfx::Size(width, views::View::GetHeightForWidth(width));
}

void PageInfoBubbleView::SetCookieInfo(const CookieInfoList& cookie_info_list) {
  // Calculate the number of cookies used by this site. |cookie_info_list|
  // should only ever have 2 items: first- and third-party cookies.
  DCHECK_EQ(cookie_info_list.size(), 2u);
  int total_allowed = 0;
  for (const auto& i : cookie_info_list) {
    total_allowed += i.allowed;
  }

  // Get the string to display the number of cookies.
  const base::string16 num_cookies_text = l10n_util::GetPluralStringFUTF16(
      IDS_PAGE_INFO_NUM_COOKIES_PARENTHESIZED, total_allowed);

  // Create the cookie button if it doesn't yet exist. This method gets called
  // each time site data is updated, so if it *does* already exist, skip this
  // part and just update the text.
  if (cookie_button_ == nullptr) {
    // Get the icon.
    PageInfo::PermissionInfo info;
    info.type = ContentSettingsType::COOKIES;
    info.setting = CONTENT_SETTING_ALLOW;
    info.is_incognito =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->IsOffTheRecord();
    const gfx::ImageSkia icon =
        PageInfoUI::GetPermissionIcon(info, GetRelatedTextColor());

    const base::string16& tooltip =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_TOOLTIP);

    cookie_button_ =
        std::make_unique<PageInfoHoverButton>(
            this, icon, IDS_PAGE_INFO_COOKIES_BUTTON_TEXT, num_cookies_text,
            VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG, tooltip,
            base::string16())
            .release();
    site_settings_view_->AddChildView(cookie_button_);
  }

  // Update the text displaying the number of allowed cookies.
  cookie_button_->SetTitleText(IDS_PAGE_INFO_COOKIES_BUTTON_TEXT,
                               num_cookies_text);

  Layout();
  SizeToContents();
}

void PageInfoBubbleView::SetPermissionInfo(
    const PermissionInfoList& permission_info_list,
    ChosenObjectInfoList chosen_object_info_list) {
  // This method is called when Page Info is constructed/displayed, then called
  // again whenever permissions/chosen objects change while the bubble is still
  // opened. Once Page Info is displaying a non-zero number of permissions, all
  // future calls to this will return early, based on the assumption that
  // permission rows won't need to be added or removed. Theoretically this
  // assumption is incorrect and it is actually possible that the number of
  // permission rows will need to change, but this should be an extremely rare
  // case that can be recovered from by closing & reopening the bubble.
  // TODO(patricialor): Investigate removing callsites to this method other than
  // the constructor.
  if (!permissions_view_->children().empty())
    return;

  views::GridLayout* layout = permissions_view_->SetLayoutManager(
      std::make_unique<views::GridLayout>());
  const bool is_list_empty =
      permission_info_list.empty() && chosen_object_info_list.empty();
  LayoutPermissionsLikeUiRow(layout, is_list_empty, kPermissionColumnSetId);

  // |ChosenObjectView| will layout itself, so just add the missing padding
  // here.
  constexpr int kChosenObjectSectionId = 1;
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int list_item_padding =
      layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL);
  const int side_margin =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).left();
  views::ColumnSet* chosen_object_set =
      layout->AddColumnSet(kChosenObjectSectionId);
  chosen_object_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                      side_margin);
  chosen_object_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                               1.0,
                               views::GridLayout::ColumnSize::kUsePreferred,
                               views::GridLayout::kFixedSize, 0);
  chosen_object_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                      side_margin);

  int min_height_for_permission_rows = 0;
  for (const auto& permission : permission_info_list) {
    std::unique_ptr<PermissionSelectorRow> selector =
        std::make_unique<PermissionSelectorRow>(
            profile_,
            web_contents() ? web_contents()->GetVisibleURL()
                           : GURL::EmptyGURL(),
            permission, layout);
    selector->AddObserver(this);
    min_height_for_permission_rows = std::max(
        min_height_for_permission_rows, selector->MinHeightForPermissionRow());
    selector_rows_.push_back(std::move(selector));
  }

  // Ensure most comboboxes are the same width by setting them all to the widest
  // combobox size, provided it does not exceed a maximum width.
  // For selected options that are over the maximum width, allow them to assume
  // their full width. If the combobox selection is changed, this may make the
  // widths inconsistent again, but that is OK since the widths will be updated
  // on the next time the bubble is opened.
  const int maximum_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUTTON_MAX_LINKABLE_WIDTH);
  int combobox_width = 0;
  for (const auto& selector : selector_rows_) {
    int curr_width = selector->GetComboboxWidth();
    if (maximum_width >= curr_width)
      combobox_width = std::max(combobox_width, curr_width);
  }
  for (const auto& selector : selector_rows_)
    selector->SetMinComboboxWidth(combobox_width);

  for (auto& object : chosen_object_info_list) {
    // Since chosen objects are presented after permissions in the same list,
    // make sure its height is the same as the permissions row's minimum height
    // plus padding.
    layout->StartRow(1.0, kChosenObjectSectionId,
                     min_height_for_permission_rows + list_item_padding);
    // The view takes ownership of the object info.
    auto object_view = std::make_unique<ChosenObjectView>(
        std::move(object),
        presenter_->GetChooserContextFromUIInfo(object->ui_info)
            ->GetObjectDisplayName(object->chooser_object->value));
    object_view->AddObserver(this);
    layout->AddView(std::move(object_view));
  }
  layout->AddPaddingRow(views::GridLayout::kFixedSize, list_item_padding);

  layout->Layout(permissions_view_);
  SizeToContents();
}

void PageInfoBubbleView::SetIdentityInfo(const IdentityInfo& identity_info) {
  std::unique_ptr<PageInfoUI::SecurityDescription> security_description =
      GetSecurityDescription(identity_info);

  SetTitle(security_description->summary);
  set_security_description_type(security_description->type);
  int text_style = views::style::STYLE_PRIMARY;
  switch (security_description->summary_style) {
    case SecuritySummaryColor::RED:
      text_style = STYLE_RED;
      break;
    case SecuritySummaryColor::GREEN:
      text_style = STYLE_GREEN;
      break;
  }
  static_cast<views::Label*>(GetBubbleFrameView()->title())
      ->SetEnabledColor(views::style::GetColor(
          *this, views::style::CONTEXT_DIALOG_TITLE, text_style));

  if (identity_info.certificate) {
    certificate_ = identity_info.certificate;

    if (identity_info.show_ssl_decision_revoke_button) {
      header_->AddResetDecisionsLabel();
    }

    // Show information about the page's certificate.
    // The text of link to the Certificate Viewer varies depending on the
    // validity of the Certificate.
    const bool valid_identity =
        (identity_info.identity_status != PageInfo::SITE_IDENTITY_STATUS_ERROR);
    base::string16 tooltip;
    if (valid_identity) {
      tooltip = l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_CERTIFICATE_VALID_LINK_TOOLTIP,
          base::UTF8ToUTF16(certificate_->issuer().GetDisplayName()));
    } else {
      tooltip = l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_CERTIFICATE_INVALID_LINK_TOOLTIP);
    }

    // Add the Certificate Section.
    const gfx::ImageSkia icon =
        PageInfoUI::GetCertificateIcon(GetRelatedTextColor());
    const base::string16 secondary_text = l10n_util::GetStringUTF16(
        valid_identity ? IDS_PAGE_INFO_CERTIFICATE_VALID_PARENTHESIZED
                       : IDS_PAGE_INFO_CERTIFICATE_INVALID_PARENTHESIZED);

    base::string16 subtitle_text;
    if (base::FeatureList::IsEnabled(features::kEvDetailsInPageInfo)) {
      // Only show the EV certificate details if there are no errors or mixed
      // content.
      if (identity_info.identity_status ==
              PageInfo::SITE_IDENTITY_STATUS_EV_CERT &&
          identity_info.connection_status ==
              PageInfo::SITE_CONNECTION_STATUS_ENCRYPTED) {
        // An EV cert is required to have an organization name and a country.
        if (!certificate_->subject().organization_names.empty() &&
            !certificate_->subject().country_name.empty()) {
          subtitle_text = l10n_util::GetStringFUTF16(
              IDS_PAGE_INFO_SECURITY_TAB_SECURE_IDENTITY_EV_VERIFIED,
              base::UTF8ToUTF16(certificate_->subject().organization_names[0]),
              base::UTF8ToUTF16(certificate_->subject().country_name));
        }
      }
    }

    // If the certificate button has been added previously, remove the old one
    // before recreating it. Re-adding it bumps it to the bottom of the
    // container, but its unlikely that the user will notice, since other things
    // are changing too.
    if (certificate_button_) {
      site_settings_view_->RemoveChildView(certificate_button_);
      auto to_delete = std::make_unique<views::View*>(certificate_button_);
    }
    certificate_button_ = site_settings_view_->AddChildView(
        std::make_unique<PageInfoHoverButton>(
            this, icon, IDS_PAGE_INFO_CERTIFICATE_BUTTON_TEXT, secondary_text,
            VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER, tooltip,
            subtitle_text)
            .release());
  }

  if (identity_info.show_change_password_buttons) {
    header_->AddPasswordReuseButtons(
        identity_info.safe_browsing_status ==
        PageInfo::SafeBrowsingStatus::
            SAFE_BROWSING_STATUS_SAVED_PASSWORD_REUSE);
  }
  details_text_ = security_description->details;
  header_->SetDetails(security_description->details);

  Layout();
  SizeToContents();
}

void PageInfoBubbleView::SetPageFeatureInfo(const PageFeatureInfo& info) {
#if defined(OS_WIN) && BUILDFLAG(ENABLE_VR)
  // For now, this has only VR settings.
  if (!info.is_vr_presentation_in_headset)
    return;

  auto* layout = page_feature_info_view_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  auto icon = std::make_unique<NonAccessibleImageView>();
  icon->SetImage(PageInfoUI::GetVrSettingsIcon(GetRelatedTextColor()));

  auto exit_button = std::make_unique<views::MdTextButton>(
      this, l10n_util::GetStringUTF16(IDS_PAGE_INFO_VR_TURN_OFF_BUTTON_TEXT));
  exit_button->SetID(VIEW_ID_PAGE_INFO_BUTTON_END_VR);
  exit_button->SetProminent(true);

  auto button = std::make_unique<HoverButton>(
      this, std::move(icon),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_VR_PRESENTING_TEXT),
      base::string16(), std::move(exit_button),
      false,  // Try not to change the row height while adding secondary view
      true);  // Secondary view can handle events.
  button->SetID(VIEW_ID_PAGE_INFO_HOVER_BUTTON_VR_PRESENTATION);

  page_feature_info_view_->AddChildView(button.release());

  Layout();
  SizeToContents();
#endif
}

void PageInfoBubbleView::LayoutPermissionsLikeUiRow(views::GridLayout* layout,
                                                    bool is_list_empty,
                                                    int column_id) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  if (is_list_empty) {
    // If nothing to show, just add padding above the separator and exit.
    layout->AddPaddingRow(views::GridLayout::kFixedSize,
                          layout_provider->GetDistanceMetric(
                              views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
    return;
  }

  const int list_item_padding =
      layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL);
  layout->AddPaddingRow(views::GridLayout::kFixedSize, list_item_padding);

  const int side_margin =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).left();
  // A permissions row will have an icon, title, and combobox, with a padding
  // column on either side to match the dialog insets. Note the combobox can be
  // variable widths depending on the text inside.
  // *----------------------------------------------*
  // |++| Icon | Permission Title     | Combobox |++|
  // *----------------------------------------------*
  views::ColumnSet* permissions_set = layout->AddColumnSet(column_id);
  permissions_set->AddPaddingColumn(views::GridLayout::kFixedSize, side_margin);
  permissions_set->AddColumn(
      views::GridLayout::CENTER, views::GridLayout::CENTER,
      views::GridLayout::kFixedSize, views::GridLayout::ColumnSize::kFixed,
      kIconColumnWidth, 0);
  permissions_set->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  permissions_set->AddColumn(views::GridLayout::LEADING,
                             views::GridLayout::CENTER, 1.0,
                             views::GridLayout::ColumnSize::kUsePreferred,
                             views::GridLayout::kFixedSize, 0);
  permissions_set->AddPaddingColumn(
      views::GridLayout::kFixedSize,
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  permissions_set->AddColumn(views::GridLayout::TRAILING,
                             views::GridLayout::FILL,
                             views::GridLayout::kFixedSize,
                             views::GridLayout::ColumnSize::kUsePreferred,
                             views::GridLayout::kFixedSize, 0);
  permissions_set->AddPaddingColumn(views::GridLayout::kFixedSize, side_margin);
}

void PageInfoBubbleView::DidChangeVisibleSecurityState() {
  presenter_->UpdateSecurityState();
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
std::unique_ptr<PageInfoUI::SecurityDescription>
PageInfoBubbleView::CreateSecurityDescriptionForPasswordReuse() const {
  std::unique_ptr<PageInfoUI::SecurityDescription> security_description(
      new PageInfoUI::SecurityDescription());
  security_description->summary_style = SecuritySummaryColor::RED;
  security_description->summary =
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY);
  auto* service = safe_browsing::ChromePasswordProtectionService::
      GetPasswordProtectionService(profile_);
  std::vector<size_t> placeholder_offsets;
  security_description->details = service->GetWarningDetailText(
      service->reused_password_account_type_for_last_shown_warning(),
      &placeholder_offsets);
  security_description->type = SecurityDescriptionType::SAFE_BROWSING;
  return security_description;
}
#endif

std::unique_ptr<views::View> PageInfoBubbleView::CreateSiteSettingsView() {
  auto site_settings_view = std::make_unique<views::View>();
  auto* box_layout =
      site_settings_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  return site_settings_view;
}

void PageInfoBubbleView::HandleMoreInfoRequest(views::View* source) {
  // The bubble closes automatically when the collected cookies dialog or the
  // certificate viewer opens. So delay handling of the link clicked to avoid
  // a crash in the base class which needs to complete the mouse event handling.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PageInfoBubbleView::HandleMoreInfoRequestAsync,
                                weak_factory_.GetWeakPtr(), source->GetID()));
}

void PageInfoBubbleView::HandleMoreInfoRequestAsync(int view_id) {
  // All switch cases require accessing web_contents(), so we check it here.
  if (web_contents() == nullptr || web_contents()->IsBeingDestroyed()) {
    return;
  }
  switch (view_id) {
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS:
      presenter_->OpenSiteSettingsView();
      break;
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG:
      // Count how often the Collected Cookies dialog is opened.
      presenter_->RecordPageInfoAction(
          PageInfo::PAGE_INFO_COOKIES_DIALOG_OPENED);
      CollectedCookiesViews::CreateAndShowForWebContents(web_contents());
      break;
    case PageInfoBubbleView::
        VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER: {
      gfx::NativeWindow top_window = web_contents()->GetTopLevelNativeWindow();
      if (certificate_ && top_window) {
        presenter_->RecordPageInfoAction(
            PageInfo::PAGE_INFO_CERTIFICATE_DIALOG_OPENED);
        ShowCertificateViewer(web_contents(), top_window, certificate_.get());
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void ShowPageInfoDialogImpl(Browser* browser,
                            content::WebContents* web_contents,
                            const GURL& virtual_url,
                            bubble_anchor_util::Anchor anchor,
                            PageInfoClosingCallback closing_callback) {
  AnchorConfiguration configuration =
      GetPageInfoAnchorConfiguration(browser, anchor);
  gfx::Rect anchor_rect =
      configuration.anchor_view ? gfx::Rect() : GetPageInfoAnchorRect(browser);
  gfx::NativeWindow parent_window = browser->window()->GetNativeWindow();
  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          configuration.anchor_view, anchor_rect, parent_window,
          browser->profile(), web_contents, virtual_url,
          std::move(closing_callback));
  bubble->SetHighlightedButton(configuration.highlighted_button);
  bubble->SetArrow(configuration.bubble_arrow);
  bubble->GetWidget()->Show();
}
