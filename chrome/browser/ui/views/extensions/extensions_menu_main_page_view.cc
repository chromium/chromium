// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_navigation_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;

// Returns the current site pointed by `web_contents`. This method should only
// be called when web contents are present.
std::u16string GetCurrentSite(content::WebContents* web_contents) {
  DCHECK(web_contents);
  const GURL& url = web_contents->GetLastCommittedURL();
  return url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
      url);
}

// Updates the `toggle_button` text based on its state.
void UpdateSiteSettingToggleText(views::ToggleButton* toggle_button) {
  bool is_on = toggle_button->GetIsOn();
  toggle_button->SetTooltipText(l10n_util::GetStringUTF16(
      is_on ? IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_ON_TOOLTIP
            : IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_OFF_TOOLTIP));
  toggle_button->SetAccessibleName(l10n_util::GetStringUTF16(
      is_on ? IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_ON_TOOLTIP
            : IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_OFF_TOOLTIP));
}

// Returns whether `site_settings_toggle_` should be on or off.
bool IsSiteSettingsToggleOn(Browser* browser,
                            content::WebContents* web_contents) {
  auto origin = web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  return PermissionsManager::Get(browser->profile())
             ->GetUserSiteSetting(origin) ==
         PermissionsManager::UserSiteSetting::kCustomizeByExtension;
}

// Returns whether `site_setting_toggle_` should be visible.
bool IsSiteSettingsToggleVisible(
    const raw_ptr<ToolbarActionsModel> toolbar_model,
    content::WebContents* web_contents) {
  return !toolbar_model->IsRestrictedUrl(web_contents->GetLastCommittedURL());
}

// Converts a view to a InstalledExtensionsMenuItemView. This cannot
// be used to *determine* if a view is an InstalledExtensionMenuItemView (it
// should only be used when the view is known to be one). It is only used as an
// extra measure to prevent bad static casts.
InstalledExtensionMenuItemView* GetAsMenuItem(views::View* view) {
  DCHECK(views::IsViewClass<InstalledExtensionMenuItemView>(view));
  return views::AsViewClass<InstalledExtensionMenuItemView>(view);
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
    : browser_(browser),
      navigation_handler_(navigation_handler),
      toolbar_model_(ToolbarActionsModel::Get(browser_->profile())) {
  views::FlexSpecification stretch_specification =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1);
  content::WebContents* web_contents = GetActiveWebContents();

  views::Builder<ExtensionsMenuMainPageView>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      // TODO(crbug.com/1390952): Add margins after adding the menu
      // items, to make sure all items are aligned.
      .AddChildren(
          // Subheader section.
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
                              .SetText(GetCurrentSite(web_contents))
                              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                              .SetTextContext(views::style::CONTEXT_LABEL)
                              .SetTextStyle(views::style::STYLE_SECONDARY)
                              .SetAllowCharacterBreak(true)
                              .SetMultiLine(true)
                              .SetProperty(views::kFlexBehaviorKey,
                                           stretch_specification)),
                  // Toggle site settings button.
                  // TODO(crbug.com/1390952): Move button under close button.
                  // This will be done as part of adding margins to the menu.
                  views::Builder<views::ToggleButton>()
                      .CopyAddressTo(&site_settings_toggle_)
                      .SetCallback(base::BindRepeating(
                          &ExtensionsMenuMainPageView::OnToggleButtonPressed,
                          base::Unretained(this)))
                      .SetVisible(IsSiteSettingsToggleVisible(toolbar_model_,
                                                              web_contents))
                      .SetIsOn(IsSiteSettingsToggleOn(browser_, web_contents)),
                  // Close button.
                  views::Builder<views::Button>(
                      views::BubbleFrameView::CreateCloseButton(
                          base::BindRepeating(
                              &ExtensionsMenuNavigationHandler::CloseBubble,
                              base::Unretained(navigation_handler_))))),
          // Request access section.
          views::Builder<RequestsAccessSection>(
              std::make_unique<RequestsAccessSection>()),
          // TODO(crbug.com/1390952): Remove. Only for testing site permissions
          // page behavior.
          views::Builder<views::LabelButton>()
              .SetText(u"Site Permissions")
              .SetCallback(base::BindRepeating(
                  &ExtensionsMenuNavigationHandler::OpenSitePermissionsPage,
                  base::Unretained(navigation_handler_))),
          // Menu items section.
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&menu_items_)
              .SetOrientation(views::BoxLayout::Orientation::kVertical))
      .BuildChildren();

  // Update toggle button text after it's build, as it depends on its state.
  UpdateSiteSettingToggleText(site_settings_toggle_);
}

void ExtensionsMenuMainPageView::CreateAndInsertMenuItem(
    std::unique_ptr<ExtensionActionViewController> action_controller,
    bool allow_pinning,
    int index) {
  auto item = std::make_unique<InstalledExtensionMenuItemView>(
      browser_, std::move(action_controller), allow_pinning);
  menu_items_->AddChildViewAt(std::move(item), index);
}

void ExtensionsMenuMainPageView::OnToggleButtonPressed() {
  // TODO(crbug.com/1390952): Update user site setting and add test.
  UpdateSiteSettingToggleText(site_settings_toggle_);
}

void ExtensionsMenuMainPageView::Update(content::WebContents* web_contents) {
  DCHECK(web_contents);

  subheader_subtitle_->SetText(GetCurrentSite(web_contents));

  site_settings_toggle_->SetVisible(
      IsSiteSettingsToggleVisible(toolbar_model_, web_contents));
  site_settings_toggle_->SetIsOn(
      IsSiteSettingsToggleOn(browser_, web_contents));
  UpdateSiteSettingToggleText(site_settings_toggle_);

  // Update menu items.
  for (auto* view : menu_items_->children()) {
    GetAsMenuItem(view)->Update();
  }
}

std::vector<InstalledExtensionMenuItemView*>
ExtensionsMenuMainPageView::GetMenuItemsForTesting() const {
  std::vector<InstalledExtensionMenuItemView*> menu_item_views;
  for (views::View* view : menu_items_->children()) {
    menu_item_views.push_back(GetAsMenuItem(view));
  }
  return menu_item_views;
}

content::WebContents* ExtensionsMenuMainPageView::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
