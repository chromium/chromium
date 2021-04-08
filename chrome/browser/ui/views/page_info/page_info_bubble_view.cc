// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reputation/safety_tip_ui_helper.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view.h"
#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_new_bubble_view.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/page_info/features.h"
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
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
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

}  // namespace

// The regular PageInfoBubbleView is not supported for internal Chrome pages and
// extension pages. Instead of the |PageInfoBubbleView|, the
// |InternalPageInfoBubbleView| is displayed.
class InternalPageInfoBubbleView : public PageInfoBubbleViewBase {
 public:
  METADATA_HEADER(InternalPageInfoBubbleView);
  // If |anchor_view| is nullptr, or has no Widget, |parent_window| may be
  // provided to ensure this bubble is closed when the parent closes.
  InternalPageInfoBubbleView(views::View* anchor_view,
                             const gfx::Rect& anchor_rect,
                             gfx::NativeView parent_window,
                             content::WebContents* web_contents,
                             const GURL& url);
  InternalPageInfoBubbleView(const InternalPageInfoBubbleView&) = delete;
  InternalPageInfoBubbleView& operator=(const InternalPageInfoBubbleView&) =
      delete;
  ~InternalPageInfoBubbleView() override;
};

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

  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

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

BEGIN_METADATA(InternalPageInfoBubbleView, PageInfoBubbleViewBase)
END_METADATA

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

  if (base::FeatureList::IsEnabled(page_info::kPageInfoV2Desktop)) {
    return new PageInfoNewBubbleView(anchor_view, anchor_rect, parent_view,
                                     profile, web_contents, url,
                                     std::move(closing_callback));
  }

  return new PageInfoBubbleView(anchor_view, anchor_rect, parent_view, profile,
                                web_contents, url, std::move(closing_callback));
}

void PageInfoBubbleView::SecurityDetailsClicked(const ui::Event& event) {
  if (GetSecurityDescriptionType() == SecurityDescriptionType::SAFETY_TIP)
    presenter_->OpenSafetyTipHelpCenterPage();
  else
    presenter_->OpenConnectionHelpCenterPage(event);
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
      layout->AddView(std::make_unique<SecurityInformationView>(side_margin));

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

    const std::u16string& tooltip =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_SETTINGS_TOOLTIP);
    site_settings_link = layout->AddView(std::make_unique<PageInfoHoverButton>(
        base::BindRepeating(
            [](PageInfoBubbleView* view) {
              view->HandleMoreInfoRequest(view->site_settings_link);
            },
            this),
        PageInfoUI::GetSiteSettingsIcon(GetRelatedTextColor()),
        IDS_PAGE_INFO_SITE_SETTINGS_LINK, std::u16string(),
        PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS,
        tooltip, std::u16string()));
  }

#if defined(OS_WIN) && BUILDFLAG(ENABLE_VR)
  layout->StartRow(views::GridLayout::kFixedSize, kColumnId);
  page_feature_info_view_ = layout->AddView(std::make_unique<views::View>());
#endif

  views::BubbleDialogDelegateView::CreateBubble(this);

  // CreateBubble() may not set our size synchronously so explicitly set it here
  // before PageInfo updates trigger child layouts.
  SetSize(GetPreferredSize());

  ui_delegate_ = std::make_unique<ChromePageInfoUiDelegate>(profile, url);
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
  presenter_->OnSitePermissionChanged(permission.type, permission.setting,
                                      permission.is_one_time);
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
  const std::u16string num_cookies_text = l10n_util::GetPluralStringFUTF16(
      IDS_PAGE_INFO_NUM_COOKIES_PARENTHESIZED, total_allowed);

  // Create the cookie button if it doesn't yet exist. This method gets called
  // each time site data is updated, so if it *does* already exist, skip this
  // part and just update the text.
  if (cookie_button_ == nullptr) {
    // Get the icon.
    PageInfo::PermissionInfo info;
    info.type = ContentSettingsType::COOKIES;
    info.setting = CONTENT_SETTING_ALLOW;
    const gfx::ImageSkia icon =
        PageInfoUI::GetPermissionIcon(info, GetRelatedTextColor());

    const std::u16string& tooltip =
        l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_TOOLTIP);

    cookie_button_ =
        std::make_unique<PageInfoHoverButton>(
            base::BindRepeating(
                [](PageInfoBubbleView* view) {
                  view->HandleMoreInfoRequest(view->cookie_button_);
                },
                this),
            icon, IDS_PAGE_INFO_COOKIES_BUTTON_TEXT, num_cookies_text,
            VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG, tooltip,
            std::u16string())
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
        std::make_unique<PermissionSelectorRow>(ui_delegate_.get(), permission,
                                                layout);
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
  SetSecurityDescriptionType(security_description->type);
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
      header_->AddResetDecisionsLabel(base::BindRepeating(
          &PageInfoBubbleView::ResetDecisionsClicked, base::Unretained(this)));
    }

    // Show information about the page's certificate.
    // The text of link to the Certificate Viewer varies depending on the
    // validity of the Certificate.
    const bool valid_identity =
        (identity_info.identity_status != PageInfo::SITE_IDENTITY_STATUS_ERROR);
    std::u16string tooltip;
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
    const std::u16string secondary_text = l10n_util::GetStringUTF16(
        valid_identity ? IDS_PAGE_INFO_CERTIFICATE_VALID_PARENTHESIZED
                       : IDS_PAGE_INFO_CERTIFICATE_INVALID_PARENTHESIZED);

    std::u16string subtitle_text;
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
            base::BindRepeating(
                [](PageInfoBubbleView* view) {
                  view->HandleMoreInfoRequest(view->certificate_button_);
                },
                this),
            icon, IDS_PAGE_INFO_CERTIFICATE_BUTTON_TEXT, secondary_text,
            VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER, tooltip,
            subtitle_text)
            .release());
  }

  if (identity_info.show_change_password_buttons) {
    header_->AddPasswordReuseButtons(
        identity_info.safe_browsing_status,
        base::BindRepeating(
            [](PageInfoBubbleView* view) {
              view->presenter_->OnChangePasswordButtonPressed();
            },
            this),
        base::BindRepeating(
            [](PageInfoBubbleView* view) {
              view->GetWidget()->Close();
              view->presenter_->OnAllowlistPasswordReuseButtonPressed();
            },
            this));
  }
  details_text_ = security_description->details;
  header_->SetDetails(
      security_description->details,
      base::BindRepeating(&PageInfoBubbleView::SecurityDetailsClicked,
                          base::Unretained(this)));

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
      base::BindRepeating(
          [](PageInfoBubbleView* view) {
            view->GetWidget()->Close();
#if BUILDFLAG(ENABLE_VR)
            vr::VrTabHelper::ExitVrPresentation();
#endif
          },
          this),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_VR_TURN_OFF_BUTTON_TEXT));
  exit_button->SetID(VIEW_ID_PAGE_INFO_BUTTON_END_VR);
  exit_button->SetProminent(true);

  auto button = std::make_unique<HoverButton>(
      views::Button::PressedCallback(), std::move(icon),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_VR_PRESENTING_TEXT),
      std::u16string(), std::move(exit_button),
      false,  // Try not to change the row height while adding secondary view
      true);  // Secondary view can handle events.
  button->SetID(VIEW_ID_PAGE_INFO_HOVER_BUTTON_VR_PRESENTATION);

  page_feature_info_view_->AddChildView(button.release());

  Layout();
  SizeToContents();
#endif
}

PageInfoUI::SecurityDescriptionType
PageInfoBubbleView::GetSecurityDescriptionType() const {
  return security_description_type_;
}

void PageInfoBubbleView::SetSecurityDescriptionType(
    const PageInfoUI::SecurityDescriptionType& type) {
  security_description_type_ = type;
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
  switch (view_id) {
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS:
      presenter_->OpenSiteSettingsView();
      break;
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG:
      presenter_->OpenCookiesDialog();
      break;
    case PageInfoBubbleView::
        VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER:
      presenter_->OpenCertificateDialog(certificate_.get());
      break;
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

DEFINE_ENUM_CONVERTERS(
    PageInfoUI::SecurityDescriptionType,
    {PageInfoUI::SecurityDescriptionType::CONNECTION, u"CONNECTION"},
    {PageInfoUI::SecurityDescriptionType::INTERNAL, u"INTERNAL"},
    {PageInfoUI::SecurityDescriptionType::SAFE_BROWSING, u"SAFE_BROWSING"},
    {PageInfoUI::SecurityDescriptionType::SAFETY_TIP, u"SAFETY_TIP"})

BEGIN_METADATA(PageInfoBubbleView, PageInfoBubbleViewBase)
ADD_PROPERTY_METADATA(PageInfoUI::SecurityDescriptionType,
                      SecurityDescriptionType)
END_METADATA
