// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_icon_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"

SideSearchIconView::SideSearchIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    Browser* browser)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate),
      browser_(browser) {
  // `template_url_service` may be null in tests.
  if (auto* template_url_service =
          TemplateURLServiceFactory::GetForProfile(browser->profile())) {
    template_url_service_observation_.Observe(template_url_service);
  }
  SetVisible(false);
}

SideSearchIconView::~SideSearchIconView() = default;

void SideSearchIconView::OnTemplateURLServiceChanged() {
  const auto* default_template_url =
      TemplateURLServiceFactory::GetForProfile(browser_->profile())
          ->GetDefaultSearchProvider();

  // Update the favicon only if the current default search provider has changed.
  if (default_template_url->id() == default_template_url_id_)
    return;
  default_template_url_id_ = default_template_url->id();
  UpdateIconImage();
}

void SideSearchIconView::UpdateImpl() {
  content::WebContents* active_contents = GetWebContents();
  if (!active_contents)
    return;

  if (active_contents->IsCrashed()) {
    SetVisible(false);
    return;
  }

  // Only show the page action button if the side panel is showable for this
  // active web contents and is not currently toggled open.
  // TODO(tluk): Setup conditions for `AnimateIn()`.
  auto* tab_contents_helper =
      SideSearchTabContentsHelper::FromWebContents(active_contents);
  const bool should_show =
      tab_contents_helper->CanShowSidePanelForCommittedNavigation() &&
      !tab_contents_helper->toggled_open();
  SetVisible(should_show);
}

void SideSearchIconView::OnExecuting(PageActionIconView::ExecuteSource source) {
  auto* side_search_browser_controller =
      BrowserView::GetBrowserViewForBrowser(browser_)->side_search_controller();
  side_search_browser_controller->ToggleSidePanel();
}

views::BubbleDialogDelegate* SideSearchIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& SideSearchIconView::GetVectorIcon() const {
  return gfx::kNoneIcon;
}

ui::ImageModel SideSearchIconView::GetSizedIconImage(int size) const {
  content::WebContents* active_contents = GetWebContents();
  if (!active_contents)
    return ui::ImageModel();

  // Attempt to synchronously get the current default search engine's favicon.
  auto* omnibox_view = search::GetOmniboxView(active_contents);
  DCHECK(omnibox_view);
  gfx::Image icon =
      omnibox_view->model()->client()->GetFaviconForDefaultSearchProvider(
          base::BindRepeating(&SideSearchIconView::OnIconFetched,
                              weak_ptr_factory_.GetWeakPtr()));
  if (icon.IsEmpty())
    return ui::ImageModel();

  // FaviconCache guarantee favicons will be of size gfx::kFaviconSize (16x16)
  // so add extra padding around them to align them vertically with the other
  // vector icons.
  DCHECK_GE(size, icon.Height());
  DCHECK_GE(size, icon.Width());
  gfx::Insets padding_border((size - icon.Height()) / 2,
                             (size - icon.Width()) / 2);

  return padding_border.IsEmpty()
             ? ui::ImageModel::FromImage(icon)
             : ui::ImageModel::FromImageSkia(
                   gfx::CanvasImageSource::CreatePadded(*icon.ToImageSkia(),
                                                        padding_border));
}

std::u16string SideSearchIconView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_TOOLTIP_SIDE_SEARCH_TOOLBAR_BUTTON_NOT_ACTIVATED);
}

void SideSearchIconView::OnIconFetched(const gfx::Image& icon) {
  // The favicon requested in the call to GetFaviconForDefaultSearchProvider()
  // will now have been cached by ChromeOmniboxClient's FaviconCache and
  // subsequent calls asking for the favicon will now return synchronously.
  UpdateIconImage();
}

BEGIN_METADATA(SideSearchIconView, PageActionIconView)
END_METADATA
