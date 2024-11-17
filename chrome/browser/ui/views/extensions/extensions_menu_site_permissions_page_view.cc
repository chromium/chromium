// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

using extensions::PermissionsManager;

// Radio buttons group id for site permissions.
constexpr int kSiteAccessButtonsId = 0;
// Indexes for each site permission radio button.
constexpr size_t kOnClickButtonIndex = 0;
constexpr size_t kOnSiteButtonIndex = 1;
constexpr size_t kOnAllSitesButtonIndex = 2;

// Returns the site access button in a site permissions `page`.
std::vector<views::RadioButton*> GetSiteAccessButtons(views::View* page) {
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons;
  page->GetViewsInGroup(kSiteAccessButtonsId, &buttons);

  std::vector<views::RadioButton*> site_access_buttons;
  site_access_buttons.reserve(buttons.size());

  base::ranges::transform(
      buttons, std::back_inserter(site_access_buttons),
      [](views::View* button) {
        return views::AsViewClass<views::RadioButton>(button);
      });
  return site_access_buttons;
}

std::u16string GetShowRequestsToggleAccessibleName(bool is_toggle_on) {
  int label_id =
      is_toggle_on
          ? IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SHOW_REQUESTS_TOGGLE_ON
          : IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SHOW_REQUESTS_TOGGLE_OFF;
  return l10n_util::GetStringUTF16(label_id);
}

// Returns the radio button text for `site_access` option.
std::u16string GetSiteAccessRadioButtonText(
    PermissionsManager::UserSiteAccess site_access,
    std::u16string current_site = std::u16string()) {
  switch (site_access) {
    case PermissionsManager::UserSiteAccess::kOnClick:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SITE_ACCESS_ON_CLICK_TEXT);
    case PermissionsManager::UserSiteAccess::kOnSite:
      return l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SITE_ACCESS_ON_SITE_TEXT,
          current_site);
    case PermissionsManager::UserSiteAccess::kOnAllSites:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SITE_ACCESS_ON_ALL_SITES_TEXT);
    default:
      NOTREACHED();
  }
}

// Returns the radio button description for `site_access` option.
std::u16string GetSiteAccessRadioButtonDescription(
    PermissionsManager::UserSiteAccess site_access) {
  switch (site_access) {
    case PermissionsManager::UserSiteAccess::kOnClick:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SITE_ACCESS_ON_CLICK_DESCRIPTION);
    case PermissionsManager::UserSiteAccess::kOnSite:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SITE_ACCESS_ON_SITE_DESCRIPTION);
    case PermissionsManager::UserSiteAccess::kOnAllSites:
      return l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SITE_ACCESS_ON_ALL_SITES_DESCRIPTION);
    default:
      NOTREACHED();
  }
}

// Returns the button index for `site_access`.
int GetSiteAccessButtonIndex(PermissionsManager::UserSiteAccess site_access) {
  switch (site_access) {
    case PermissionsManager::UserSiteAccess::kOnClick:
      return kOnClickButtonIndex;
    case PermissionsManager::UserSiteAccess::kOnSite:
      return kOnSiteButtonIndex;
    case PermissionsManager::UserSiteAccess::kOnAllSites:
      return kOnAllSitesButtonIndex;
  }
}

// Returns the icon for the setting button.
std::unique_ptr<views::ImageView> GetSettingsButtonIcon(int icon_size) {
  return std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
      vector_icons::kSubmenuArrowChromeRefreshIcon, ui::kColorIconSecondary,
      icon_size));
}

}  // namespace

ExtensionsMenuSitePermissionsPageView::ExtensionsMenuSitePermissionsPageView(
    Browser* browser,
    extensions::ExtensionId extension_id,
    ExtensionsMenuHandler* menu_handler)
    : browser_(browser), extension_id_(extension_id) {
  // TODO(crbug.com/40879945): Same stretch specification as
  // ExtensionsMenuMainPageView. Move to a shared file.
  views::FlexSpecification stretch_specification =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1);

  views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  const gfx::Insets dialog_insets =
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG);
  // Back button should use a vector image button, and this value should be the
  // same as the border added by the image button factory.
  const gfx::Insets back_button_border = layout_provider->GetInsetsMetric(
      views::InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON);

  ChromeLayoutProvider* const chrome_layout_provider =
      ChromeLayoutProvider::Get();
  const int icon_size = chrome_layout_provider->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE);
  const int horizontal_spacing = chrome_layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);
  const int vertical_spacing = chrome_layout_provider->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE);
  // This value must be the same as the `HoverButton` vertical margin.
  const int hover_button_vertical_spacing =
      chrome_layout_provider->GetDistanceMetric(
          DISTANCE_CONTROL_LIST_VERTICAL) /
      2;

  // Views that need configuration after construction (e.g access size after a
  // separate view is constructed).
  views::Label* toggle_label;

  const auto create_separator_builder =
      [dialog_insets, vertical_spacing, hover_button_vertical_spacing](
          bool full_width, bool is_bottom_hover_button = false) {
        const int horizontal_margin = full_width ? 0 : dialog_insets.left();
        const int bottom_margin =
            is_bottom_hover_button
                ? vertical_spacing - hover_button_vertical_spacing
                : vertical_spacing;

        return views::Builder<views::Separator>().SetProperty(
            views::kMarginsKey,
            gfx::Insets::TLBR(vertical_spacing, horizontal_margin,
                              bottom_margin, horizontal_margin));
      };

  const auto create_radio_button_builder =
      [=](PermissionsManager::UserSiteAccess site_access) {
        return views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            // Add dialog horizontal margins, and top margin to separate the
            // items.
            .SetProperty(
                views::kMarginsKey,
                gfx::Insets::TLBR(vertical_spacing, dialog_insets.left(), 0,
                                  dialog_insets.right()))
            .AddChildren(
                views::Builder<views::RadioButton>()
                    .SetText(GetSiteAccessRadioButtonText(site_access))
                    .SetLabelStyle(views::style::STYLE_BODY_3)
                    .SetEnabledTextColorIds(kColorExtensionsMenuText)
                    .SetGroup(kSiteAccessButtonsId)
                    // To align the radio button icon under the header back
                    // button we add the back button border to the
                    // left of the icon.
                    .SetProperty(
                        views::kMarginsKey,
                        gfx::Insets::TLBR(0, back_button_border.left(), 0, 0))
                    // To align the radio button text under the header label
                    // we add the back button border and horizontal spacing in
                    // between the radio button icon and label.
                    .SetImageLabelSpacing(back_button_border.right() +
                                          horizontal_spacing)
                    .SetCallback(base::BindRepeating(
                        &ExtensionsMenuHandler::OnSiteAccessSelected,
                        base::Unretained(menu_handler), extension_id,
                        site_access)),
                views::Builder<views::Label>()
                    .SetText(GetSiteAccessRadioButtonDescription(site_access))
                    .SetTextStyle(views::style::STYLE_BODY_5)
                    .SetEnabledColorId(kColorExtensionsMenuSecondaryText)
                    .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                    .SetMultiLine(true)
                    .SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
                        0, icon_size + back_button_border.left() +
                               back_button_border.right() +
                               horizontal_spacing))));
      };

  views::Builder<ExtensionsMenuSitePermissionsPageView>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      .AddChildren(
          // Header.
          views::Builder<views::FlexLayoutView>()
              // Add top dialog margins, since its the first element, and
              // horizontal dialog margins. Vertical margins will be handled
              // in between the contents.
              .SetInteriorMargin(gfx::Insets::TLBR(dialog_insets.top(),
                                                   dialog_insets.left(), 0,
                                                   dialog_insets.right()))
              .AddChildren(
                  // Back button.
                  views::Builder<views::ImageButton>(
                      views::CreateVectorImageButtonWithNativeTheme(
                          base::BindRepeating(
                              &ExtensionsMenuHandler::OpenMainPage,
                              base::Unretained(menu_handler)),
                          vector_icons::kArrowBackIcon, icon_size))
                      .SetTooltipText(
                          l10n_util::GetStringUTF16(IDS_ACCNAME_BACK))
                      .SetAccessibleName(
                          l10n_util::GetStringUTF16(IDS_ACCNAME_BACK))
                      .CustomConfigure(
                          base::BindOnce([](views::ImageButton* view) {
                            InstallCircleHighlightPathGenerator(view);
                          })),
                  // Extension name.
                  views::Builder<views::FlexLayoutView>()
                      .SetOrientation(views::LayoutOrientation::kHorizontal)
                      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
                      .SetProperty(views::kFlexBehaviorKey,
                                   stretch_specification)
                      .AddChildren(
                          views::Builder<views::ImageView>()
                              .CopyAddressTo(&extension_icon_)
                              .SetProperty(views::kMarginsKey,
                                           gfx::Insets::TLBR(
                                               0, horizontal_spacing, 0, 0)),
                          views::Builder<views::Label>()
                              .CopyAddressTo(&extension_name_)
                              .SetTextStyle(views::style::STYLE_HEADLINE_4)
                              .SetEnabledColorId(
                                  kColorExtensionsMenuSecondaryText)
                              .SetProperty(views::kMarginsKey,
                                           gfx::Insets::TLBR(
                                               0, horizontal_spacing, 0, 0))),
                  // Close button.
                  views::Builder<views::Button>(
                      views::BubbleFrameView::CreateCloseButton(
                          base::BindRepeating(
                              &ExtensionsMenuHandler::CloseBubble,
                              base::Unretained(menu_handler))))),
          create_separator_builder(/*full_width=*/true,
                                   /*is_bottom_hover_button=*/false),
          // Content.
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .AddChildren(
                  // Site access section.
                  views::Builder<views::Label>()
                      // Add dialog horizontal margins. Vertical margins are
                      // handled by surrounding views.
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::VH(0, dialog_insets.left()))
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SITE_ACCESS_LABEL))
                      .SetTextStyle(views::style::STYLE_BODY_3_EMPHASIS)
                      .SetEnabledColorId(kColorExtensionsMenuText)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                  create_radio_button_builder(
                      PermissionsManager::UserSiteAccess::kOnClick),
                  create_radio_button_builder(
                      PermissionsManager::UserSiteAccess::kOnSite),
                  create_radio_button_builder(
                      PermissionsManager::UserSiteAccess::kOnAllSites),
                  // Requests in toolbar toggle.
                  create_separator_builder(/*full_width=*/false),
                  views::Builder<views::FlexLayoutView>()
                      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
                      // Add dialog horizontal margins. Vertical margins are
                      // handled by separators.
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::VH(0, dialog_insets.left()))
                      .AddChildren(
                          views::Builder<views::Label>()
                              .CopyAddressTo(&toggle_label)
                              .SetText(l10n_util::GetStringUTF16(
                                  IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SHOW_REQUESTS_LABEL))
                              .SetTextStyle(views::style::STYLE_BODY_3_EMPHASIS)
                              .SetEnabledColorId(kColorExtensionsMenuText)
                              .SetProperty(views::kFlexBehaviorKey,
                                           stretch_specification)
                              .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                          views::Builder<views::ToggleButton>()
                              .CopyAddressTo(&show_requests_toggle_)
                              .SetCallback(base::BindRepeating(
                                  [](const extensions::ExtensionId&
                                         extension_id,
                                     views::ToggleButton* toggle_button,
                                     base::RepeatingCallback<void(
                                         const extensions::ExtensionId&, bool)>
                                         callback) {
                                    callback.Run(extension_id,
                                                 toggle_button->GetIsOn());
                                  },
                                  extension_id_, show_requests_toggle_,
                                  base::BindRepeating(
                                      &ExtensionsMenuHandler::
                                          OnShowRequestsTogglePressed,
                                      base::Unretained(menu_handler))))),
                  // Settings button.
                  create_separator_builder(/*full_width=*/false,
                                           /*is_bottom_hover_button=*/true),
                  views::Builder<HoverButton>(
                      std::make_unique<HoverButton>(
                          base::BindRepeating(
                              [](Browser* browser,
                                 extensions::ExtensionId extension_id) {
                                chrome::ShowExtensions(browser, extension_id);
                              },
                              browser, extension_id_),
                          /*icon_view=*/nullptr,
                          l10n_util::GetStringUTF16(
                              IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SETTINGS_BUTTON),
                          /*subtitle=*/std::u16string(),
                          GetSettingsButtonIcon(icon_size)))
                      .SetTitleTextStyle(views::style::STYLE_BODY_3_EMPHASIS,
                                         ui::kColorDialogBackground,
                                         kColorExtensionsMenuText)
                      // Align the hover button text by adding the dialog
                      // horizontal margins for the horizontal borders.
                      .SetBorder(views::CreateEmptyBorder(
                          gfx::Insets::VH(0, dialog_insets.left())))
                      // Add bottom dialog margins since it's the last
                      // element.
                      .SetProperty(
                          views::kMarginsKey,
                          gfx::Insets::TLBR(0, 0,
                                            dialog_insets.bottom() -
                                                hover_button_vertical_spacing,
                                            0))
                      .SetTooltipText(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SETTINGS_BUTTON_TOOLTIP))))

      .BuildChildren();

  // Align the show requests toggle vertically with the label by getting the
  // label height after construction.
  show_requests_toggle_->SetPreferredSize(
      gfx::Size(show_requests_toggle_->GetPreferredSize().width(),
                toggle_label->GetLineHeight()));
}

void ExtensionsMenuSitePermissionsPageView::Update(
    const std::u16string& extension_name,
    const ui::ImageModel& extension_icon,
    const std::u16string& current_site,
    PermissionsManager::UserSiteAccess user_site_access,
    bool is_show_requests_toggle_on,
    bool is_on_site_enabled,
    bool is_on_all_sites_enabled) {
  extension_icon_->SetImage(extension_icon);
  extension_name_->SetText(extension_name);

  // Update the site access buttons with new `user_site_access` and
  // `current_site`.
  int new_selected_index = GetSiteAccessButtonIndex(user_site_access);
  std::vector<views::RadioButton*> site_access_buttons =
      GetSiteAccessButtons(this);
  for (int i = 0; i < static_cast<int>(site_access_buttons.size()); ++i) {
    site_access_buttons[i]->SetChecked(i == new_selected_index);
    if (i == kOnSiteButtonIndex) {
      site_access_buttons[i]->SetText(GetSiteAccessRadioButtonText(
          PermissionsManager::UserSiteAccess::kOnSite, current_site));
    }
  }

  // Enable the site access buttons accordingly. The extension is guaranteed to
  // at least have "on click" enabled when this page is opened.
  site_access_buttons[kOnSiteButtonIndex]->SetEnabled(is_on_site_enabled);
  site_access_buttons[kOnAllSitesButtonIndex]->SetEnabled(
      is_on_all_sites_enabled);

  UpdateShowRequestsToggle(is_show_requests_toggle_on);
}

void ExtensionsMenuSitePermissionsPageView::UpdateShowRequestsToggle(
    bool is_on) {
  show_requests_toggle_->SetIsOn(is_on);
  show_requests_toggle_->GetViewAccessibility().SetName(
      GetShowRequestsToggleAccessibleName(is_on));
}

views::RadioButton*
ExtensionsMenuSitePermissionsPageView::GetSiteAccessButtonForTesting(
    PermissionsManager::UserSiteAccess site_access) {
  std::vector<views::RadioButton*> site_access_buttons =
      GetSiteAccessButtons(this);
  return site_access_buttons[GetSiteAccessButtonIndex(site_access)];
}

BEGIN_METADATA(ExtensionsMenuSitePermissionsPageView)
END_METADATA
