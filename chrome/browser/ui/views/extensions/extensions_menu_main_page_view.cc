// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_navigation_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

using PermissionsManager = extensions::PermissionsManager;

// Updates the `toggle_button` text based on its state.
std::u16string GetSiteSettingToggleText(bool is_on) {
  int label_id = is_on ? IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_ON_TOOLTIP
                       : IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_OFF_TOOLTIP;
  return l10n_util::GetStringUTF16(label_id);
}

// Converts a view to a ExtensionMenuItemView. This cannot be used to
// *determine* if a view is an ExtensionMenuItemView (it should only be used
// when the view is known to be one). It is only used as an extra measure to
// prevent bad static casts.
ExtensionMenuItemView* GetAsMenuItem(views::View* view) {
  DCHECK(views::IsViewClass<ExtensionMenuItemView>(view));
  return views::AsViewClass<ExtensionMenuItemView>(view);
}

// Returns the ExtensionMenuItemView corresponding to `action_id` if
// it is a children of `parent_view`. The children of the parent view must be
// ExtensionMenuItemView, otherwise it will DCHECK.
ExtensionMenuItemView* GetMenuItem(
    views::View* parent_view,
    const ToolbarActionsModel::ActionId& action_id) {
  for (auto* view : parent_view->children()) {
    auto* item_view = GetAsMenuItem(view);
    if (item_view->view_controller()->GetId() == action_id) {
      return item_view;
    }
  }
  return nullptr;
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
  // This is set so that the extensions menu doesn't fall outside the monitor in
  // a maximized window in 1024x768. See https://crbug.com/1096630.
  // TODO(crbug.com/1413883): Consider making the height dynamic.
  constexpr int kMaxExtensionButtonsHeightDp = 448;
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
                              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                              .SetTextContext(views::style::CONTEXT_LABEL)
                              .SetTextStyle(views::style::STYLE_SECONDARY)
                              .SetAllowCharacterBreak(true)
                              .SetMultiLine(true)
                              .SetProperty(views::kFlexBehaviorKey,
                                           stretch_specification)),
                  // TODO(crbug.com/1390952): Move setting and toggle button
                  // under close button. This will be done as part of adding
                  // margins to the menu.
                  // Setting button.
                  views::Builder<views::ImageButton>(
                      views::CreateVectorImageButtonWithNativeTheme(
                          base::BindRepeating(
                              [](Browser* browser) {
                                chrome::ShowExtensions(browser);
                              },
                              browser_),
                          vector_icons::kSettingsIcon))
                      .SetAccessibleName(
                          l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS))
                      .CustomConfigure(
                          base::BindOnce([](views::ImageButton* view) {
                            view->SizeToPreferredSize();
                            InstallCircleHighlightPathGenerator(view);
                          })),
                  // Toggle site settings button.
                  views::Builder<views::ToggleButton>()
                      .CopyAddressTo(&site_settings_toggle_)
                      .SetCallback(base::BindRepeating(
                          &ExtensionsMenuMainPageView::OnToggleButtonPressed,
                          base::Unretained(this))),
                  // Close button.
                  views::Builder<views::Button>(
                      views::BubbleFrameView::CreateCloseButton(
                          base::BindRepeating(
                              &ExtensionsMenuNavigationHandler::CloseBubble,
                              base::Unretained(navigation_handler_))))),
          // Contents.
          views::Builder<views::Separator>(),
          views::Builder<views::ScrollView>()
              .ClipHeightTo(0, kMaxExtensionButtonsHeightDp)
              .SetDrawOverflowIndicator(false)
              .SetHorizontalScrollBarMode(
                  views::ScrollView::ScrollBarMode::kDisabled)
              .SetContents(
                  views::Builder<views::BoxLayoutView>()
                      .SetOrientation(views::BoxLayout::Orientation::kVertical)
                      .AddChildren(
                          // Request access section.
                          views::Builder<RequestsAccessSection>(
                              std::make_unique<RequestsAccessSection>()),
                          // Menu items section.
                          views::Builder<views::BoxLayoutView>()
                              .CopyAddressTo(&menu_items_)
                              .SetOrientation(
                                  views::BoxLayout::Orientation::kVertical))))

      .BuildChildren();
}

void ExtensionsMenuMainPageView::CreateAndInsertMenuItem(
    std::unique_ptr<ExtensionActionViewController> action_controller,
    extensions::ExtensionId extension_id,
    ExtensionMenuItemView::SiteAccessToggleState site_access_toggle_state,
    ExtensionMenuItemView::SitePermissionsButtonState
        site_permissions_button_state,
    int index) {
  auto item = std::make_unique<ExtensionMenuItemView>(
      browser_, std::move(action_controller),
      // TODO(crbug.com/1390952): Create callback that grants/withhelds site
      // access when toggling the site access toggle.
      base::RepeatingClosure(base::NullCallback()),
      base::BindRepeating(
          &ExtensionsMenuNavigationHandler::OpenSitePermissionsPage,
          base::Unretained(navigation_handler_), extension_id));
  item->Update(site_access_toggle_state, site_permissions_button_state);
  menu_items_->AddChildViewAt(std::move(item), index);
}

void ExtensionsMenuMainPageView::RemoveMenuItem(
    const ToolbarActionsModel::ActionId& action_id) {
  views::View* item = GetMenuItem(menu_items_, action_id);
  menu_items_->RemoveChildViewT(item);
}

void ExtensionsMenuMainPageView::OnToggleButtonPressed() {
  const url::Origin& origin =
      GetActiveWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  PermissionsManager::UserSiteSetting site_setting =
      site_settings_toggle_->GetIsOn()
          ? PermissionsManager::UserSiteSetting::kCustomizeByExtension
          : PermissionsManager::UserSiteSetting::kBlockAllExtensions;

  PermissionsManager::Get(browser_->profile())
      ->UpdateUserSiteSetting(origin, site_setting);
}

void ExtensionsMenuMainPageView::Update(std::u16string current_site,
                                        bool is_site_settings_toggle_visible,
                                        bool is_site_settings_toggle_on) {
  subheader_subtitle_->SetText(current_site);

  site_settings_toggle_->SetVisible(is_site_settings_toggle_visible);
  site_settings_toggle_->SetIsOn(is_site_settings_toggle_on);
  site_settings_toggle_->SetTooltipText(
      GetSiteSettingToggleText(is_site_settings_toggle_on));
  site_settings_toggle_->SetAccessibleName(
      GetSiteSettingToggleText(is_site_settings_toggle_on));
}

std::vector<ExtensionMenuItemView*> ExtensionsMenuMainPageView::GetMenuItems()
    const {
  std::vector<ExtensionMenuItemView*> menu_item_views;
  for (views::View* view : menu_items_->children()) {
    menu_item_views.push_back(GetAsMenuItem(view));
  }
  return menu_item_views;
}

content::WebContents* ExtensionsMenuMainPageView::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

BEGIN_METADATA(ExtensionsMenuMainPageView, views::View)
END_METADATA
