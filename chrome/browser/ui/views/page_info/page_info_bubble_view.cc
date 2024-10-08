// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include <memory>

#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_security_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/page_info/core/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"

using bubble_anchor_util::AnchorConfiguration;
using bubble_anchor_util::GetPageInfoAnchorConfiguration;
using bubble_anchor_util::GetPageInfoAnchorRect;

// The regular PageInfoBubbleView is not supported for internal Chrome pages and
// extension pages. Instead of the |PageInfoBubbleView|, the
// |InternalPageInfoBubbleView| is displayed.
class InternalPageInfoBubbleView : public PageInfoBubbleViewBase {
  METADATA_HEADER(InternalPageInfoBubbleView, PageInfoBubbleViewBase)

 public:
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
  } else {
    CHECK(url.SchemeIs(content::kChromeUIScheme));
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

  // TODO(crbug.com/343325197) Remove this SizeToContents() once this bug is
  // fixed.
  SizeToContents();
}

InternalPageInfoBubbleView::~InternalPageInfoBubbleView() = default;

BEGIN_METADATA(InternalPageInfoBubbleView)
END_METADATA

PageInfoBubbleView::PageInfoBubbleView(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    content::WebContents* associated_web_contents,
    const GURL& url,
    base::OnceClosure initialized_callback,
    PageInfoClosingCallback closing_callback,
    bool allow_about_this_site)
    : PageInfoBubbleViewBase(anchor_view,
                             anchor_rect,
                             parent_window,
                             PageInfoBubbleViewBase::BUBBLE_PAGE_INFO,
                             associated_web_contents),
      closing_callback_(std::move(closing_callback)) {
  DCHECK(closing_callback_);
  DCHECK(web_contents());
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // In Harmony, the last view is a HoverButton, which overrides the bottom
  // dialog inset in favor of its own. Note the multi-button value is used here
  // assuming that the "Cookies" & "Site settings" buttons will always be shown.
  const int bottom_margin =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  const int top_margin =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).top();
  set_margins(gfx::Insets::TLBR(top_margin, 0, bottom_margin, 0));
  ui_delegate_ =
      std::make_unique<ChromePageInfoUiDelegate>(web_contents(), url);
  presenter_ = std::make_unique<PageInfo>(
      std::make_unique<ChromePageInfoDelegate>(web_contents()), web_contents(),
      url);
  if (base::FeatureList::IsEnabled(page_info::kPageInfoHistoryDesktop)) {
    history_controller_ =
        std::make_unique<PageInfoHistoryController>(web_contents(), url);
  }
  view_factory_ = std::make_unique<PageInfoViewFactory>(
      presenter_.get(), ui_delegate_.get(), this, history_controller_.get(),
      allow_about_this_site);

  SetShowTitle(false);
  SetShowCloseButton(false);
  // The title isn't visible, it is set for a11y purposes and the actual visible
  // title is a custom label in the content view.
  SetTitle(presenter_->GetSubjectNameForDisplay());

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  std::unique_ptr<views::View> main_page_view =
      view_factory_->CreateMainPageView(std::move(initialized_callback));
  main_page_view->SetID(PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW);
  page_container_ = AddChildView(
      std::make_unique<PageSwitcherView>(std::move(main_page_view)));

  views::BubbleDialogDelegateView::CreateBubble(this);
}

PageInfoBubbleView::~PageInfoBubbleView() {
  page_container_ = nullptr;
  RemoveAllChildViews();
}

// static
views::BubbleDialogDelegateView* PageInfoBubbleView::CreatePageInfoBubble(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeWindow parent_window,
    content::WebContents* web_contents,
    const GURL& url,
    base::OnceClosure initialized_callback,
    PageInfoClosingCallback closing_callback,
    bool allow_about_this_site) {
  DCHECK(web_contents);
  gfx::NativeView parent_view = platform_util::GetViewForWindow(parent_window);

  if (PageInfo::IsFileOrInternalPage(url) ||
      url.SchemeIs(extensions::kExtensionScheme) ||
      url.SchemeIs(dom_distiller::kDomDistillerScheme)) {
    return new InternalPageInfoBubbleView(anchor_view, anchor_rect, parent_view,
                                          web_contents, url);
  }

  return new PageInfoBubbleView(
      anchor_view, anchor_rect, parent_view, web_contents, url,
      std::move(initialized_callback), std::move(closing_callback),
      allow_about_this_site);
}

void PageInfoBubbleView::OpenMainPage(base::OnceClosure initialized_callback) {
  std::unique_ptr<views::View> main_page_view =
      view_factory_->CreateMainPageView(std::move(initialized_callback));
  main_page_view->SetID(PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW);
  page_container_->SwitchToPage(std::move(main_page_view));
}

void PageInfoBubbleView::OpenSecurityPage() {
  presenter_->RecordPageInfoAction(
      page_info::PAGE_INFO_SECURITY_DETAILS_OPENED);
  std::unique_ptr<views::View> security_page_view =
      view_factory_->CreateSecurityPageView();
  security_page_view->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW);
  page_container_->SwitchToPage(std::move(security_page_view));
  AnnouncePageOpened(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_SUBPAGE_HEADER));
}

void PageInfoBubbleView::OpenPermissionPage(ContentSettingsType type) {
  presenter_->RecordPageInfoAction(
      page_info::PAGE_INFO_PERMISSION_DIALOG_OPENED);
  std::unique_ptr<views::View> permissions_page_view =
      view_factory_->CreatePermissionPageView(type, web_contents());
  permissions_page_view->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW);
  page_container_->SwitchToPage(std::move(permissions_page_view));
  AnnouncePageOpened(PageInfoUI::PermissionTypeToUIString(type));
}

void PageInfoBubbleView::OpenAdPersonalizationPage() {
  presenter_->RecordPageInfoAction(
      page_info::PAGE_INFO_AD_PERSONALIZATION_PAGE_OPENED);
  std::unique_ptr<views::View> ad_personalization_page_view =
      view_factory_->CreateAdPersonalizationPageView();
  ad_personalization_page_view->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW);
  page_container_->SwitchToPage(std::move(ad_personalization_page_view));
  AnnouncePageOpened(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_AD_PRIVACY_HEADER));
}

void PageInfoBubbleView::OpenCookiesPage() {
  presenter_->OnCookiesPageOpened();
  std::unique_ptr<views::View> cookies_page_view =
      view_factory_->CreateCookiesPageView();
  cookies_page_view->SetID(PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW);
  page_container_->SwitchToPage(std::move(cookies_page_view));
  AnnouncePageOpened(l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES));
}

void PageInfoBubbleView::CloseBubble() {
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void PageInfoBubbleView::DidChangeVisibleSecurityState() {
  presenter_->UpdateSecurityState();
}

void PageInfoBubbleView::OnWidgetDestroying(views::Widget* widget) {
  PageInfoBubbleViewBase::OnWidgetDestroying(widget);

  // This method mostly shouldn't be re-entrant but there are a few cases where
  // it can be (see crbug/966308). In that case, we have already run the closing
  // callback so should not attempt to do it again. As there will always be a
  // |closing_callback_|, this is also used to ensure that the |presenter_| is
  // informed exactly once.
  if (closing_callback_) {
    bool reload_prompt;
    presenter_->OnUIClosing(&reload_prompt);

    std::move(closing_callback_).Run(widget->closed_reason(), reload_prompt);
  }
}

void PageInfoBubbleView::WebContentsDestroyed() {
  PageInfoBubbleViewBase::WebContentsDestroyed();
  weak_factory_.InvalidateWeakPtrs();
}

gfx::Size PageInfoBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (page_container_ == nullptr) {
    return views::View::CalculatePreferredSize(available_size);
  }

  int width = PageInfoViewFactory::kMinBubbleWidth;
  if (page_container_) {
    width = std::max(width, page_container_->GetPreferredSize().width());
    width = std::min(width, PageInfoViewFactory::kMaxBubbleWidth);
  }
  return gfx::Size(width,
                   GetLayoutManager()->GetPreferredHeightForWidth(this, width));
}

void PageInfoBubbleView::ChildPreferredSizeChanged(views::View* child) {
  // TODO(crbug.com/343325197) Remove this SizeToContents() once this bug is
  // fixed.
  SizeToContents();
}

void PageInfoBubbleView::AnnouncePageOpened(std::u16string announcement) {
  // Announce that the subpage was opened to inform the user about the changes
  // in the UI.
  GetViewAccessibility().AnnounceText(announcement);

  // Focus the back button by default to ensure that focus is set when new
  // content is displayed.
  auto* back_button = page_container_->GetViewByID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_BACK_BUTTON);
  back_button->RequestFocus();
}

void ShowPageInfoDialogImpl(Browser* browser,
                            content::WebContents* web_contents,
                            const GURL& virtual_url,
                            bubble_anchor_util::Anchor anchor,
                            base::OnceClosure initialized_callback,
                            PageInfoClosingCallback closing_callback) {
  AnchorConfiguration configuration =
      GetPageInfoAnchorConfiguration(browser, anchor);
  gfx::Rect anchor_rect =
      configuration.anchor_view ? gfx::Rect() : GetPageInfoAnchorRect(browser);
  gfx::NativeWindow parent_window = browser->window()->GetNativeWindow();
  DCHECK(web_contents);
  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          configuration.anchor_view, anchor_rect, parent_window, web_contents,
          virtual_url, std::move(initialized_callback),
          std::move(closing_callback), /*allow_about_this_site=*/true);
  bubble->SetHighlightedButton(configuration.highlighted_button);
  bubble->SetArrow(configuration.bubble_arrow);
  bubble->GetWidget()->Show();
}

BEGIN_METADATA(PageInfoBubbleView)
END_METADATA
