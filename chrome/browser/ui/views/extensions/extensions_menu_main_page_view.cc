// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_navigation_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

// Returns the current site pointed by `web_contents`. This method should only
// be called when web contents are present.
std::u16string GetCurrentSite(content::WebContents* web_contents) {
  DCHECK(web_contents);
  const GURL& url = web_contents->GetLastCommittedURL();
  return url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
      url);
}

}  // namespace

class RequestsAccessSection : public views::BoxLayoutView {
 public:
  RequestsAccessSection();
  RequestsAccessSection(const RequestsAccessSection&) = delete;
  const RequestsAccessSection& operator=(const RequestsAccessSection&) = delete;
  ~RequestsAccessSection() override = default;

 private:
  raw_ptr<views::View> extension_items_;
};

BEGIN_VIEW_BUILDER(/* No Export */, RequestsAccessSection, views::BoxLayoutView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* No Export */, RequestsAccessSection)

RequestsAccessSection::RequestsAccessSection() {
  views::Builder<RequestsAccessSection>(this)
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      .SetVisible(false)
      // TODO(crbug.com/1390952): After adding margins, compute radius from a
      // variable or create a const variable.
      .SetBackground(views::CreateThemedRoundedRectBackground(
          kColorExtensionsMenuHighlightedBackground, 4))
      .AddChildren(
          // Header explaining the section.
          views::Builder<views::Label>()
              .SetText(l10n_util::GetStringUTF16(
                  IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_TITLE))
              .SetTextContext(ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
              .SetTextStyle(views::style::STYLE_EMPHASIZED)
              .SetHorizontalAlignment(gfx::ALIGN_LEFT),
          // Empty container for the extensions requesting access. Items will be
          // populated later.
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .CopyAddressTo(&extension_items_))
      .BuildChildren();
  // TODO(crbug.com/1390952): Populate `extension_items_` with extensions
  // requesting access.
}

ExtensionsMenuMainPageView::ExtensionsMenuMainPageView(
    Browser* browser,
    ExtensionsMenuNavigationHandler* navigation_handler)
    : browser_(browser), navigation_handler_(navigation_handler) {
  views::FlexSpecification stretch_specification =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1);

  views::Builder<ExtensionsMenuMainPageView>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      // TODO(crbug.com/1390952): Add margins after adding the menu
      // items, to make sure all items are aligned.
      .AddChildren(
          // Subheader.
          views::Builder<views::FlexLayoutView>()
              .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
              .SetProperty(views::kFlexBehaviorKey, stretch_specification)
              .SetVisible(true)
              .AddChildren(
                  views::Builder<views::FlexLayoutView>()
                      .SetOrientation(views::LayoutOrientation::kVertical)
                      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
                      .SetProperty(views::kFlexBehaviorKey,
                                   stretch_specification)
                      .AddChildren(
                          views::Builder<views::Label>()
                              .SetText(l10n_util::GetStringUTF16(
                                  IDS_EXTENSIONS_MENU_TITLE))
                              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                              .SetTextContext(
                                  views::style::CONTEXT_DIALOG_TITLE)
                              .SetTextStyle(views::style::STYLE_SECONDARY),
                          views::Builder<views::Label>()
                              .CopyAddressTo(&subheader_subtitle_)
                              .SetText(GetCurrentSite(GetActiveWebContents()))
                              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                              .SetTextContext(views::style::CONTEXT_LABEL)
                              .SetTextStyle(views::style::STYLE_SECONDARY)
                              .SetAllowCharacterBreak(true)
                              .SetMultiLine(true)
                              .SetProperty(views::kFlexBehaviorKey,
                                           stretch_specification)),
                  views::Builder<views::Button>(
                      views::BubbleFrameView::CreateCloseButton(
                          base::BindRepeating(
                              &ExtensionsMenuNavigationHandler::CloseBubble,
                              base::Unretained(navigation_handler_))))),
          // Request access section.
          views::Builder<RequestsAccessSection>(
              std::make_unique<RequestsAccessSection>()))
      .BuildChildren();

  browser_->tab_strip_model()->AddObserver(this);
}

void ExtensionsMenuMainPageView::Update() {
  content::WebContents* web_contents = GetActiveWebContents();
  if (web_contents)
    subheader_subtitle_->SetText(GetCurrentSite(web_contents));
}

void ExtensionsMenuMainPageView::TabChangedAt(content::WebContents* contents,
                                              int index,
                                              TabChangeType change_type) {
  Update();
}

void ExtensionsMenuMainPageView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  Update();
}

content::WebContents* ExtensionsMenuMainPageView::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
