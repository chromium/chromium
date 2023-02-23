// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_site_permissions_page_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_navigation_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

ExtensionsMenuSitePermissionsPageView::ExtensionsMenuSitePermissionsPageView(
    Browser* browser,
    std::u16string extension_name,
    ui::ImageModel extension_icon,
    extensions::ExtensionId extension_id,
    ExtensionsMenuNavigationHandler* navigation_handler)
    : extension_id_(extension_id) {
  // TODO(crbug.com/1390952): Same stretch specification as
  // ExtensionsMenuMainPageView. Move to a shared file.
  views::FlexSpecification stretch_specification =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1);

  views::Builder<ExtensionsMenuSitePermissionsPageView>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      // TODO(crbug.com/1390952): Add margins after adding the menu
      // items, to make sure all items are aligned.
      .AddChildren(
          // Subheader.
          views::Builder<views::FlexLayoutView>()
              .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
              .SetProperty(views::kFlexBehaviorKey, stretch_specification)
              .AddChildren(
                  // Back button.
                  views::Builder<views::ImageButton>(
                      views::CreateVectorImageButtonWithNativeTheme(
                          base::BindRepeating(
                              &ExtensionsMenuNavigationHandler::OpenMainPage,
                              base::Unretained(navigation_handler)),
                          vector_icons::kArrowBackIcon))
                      .SetTooltipText(
                          l10n_util::GetStringUTF16(IDS_ACCNAME_BACK))
                      .SetAccessibleName(
                          l10n_util::GetStringUTF16(IDS_ACCNAME_BACK))
                      .CustomConfigure(
                          base::BindOnce([](views::ImageButton* view) {
                            view->SizeToPreferredSize();
                            InstallCircleHighlightPathGenerator(view);
                          })),
                  // Extension name.
                  views::Builder<views::FlexLayoutView>()
                      .SetOrientation(views::LayoutOrientation::kHorizontal)
                      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
                      .SetProperty(views::kFlexBehaviorKey,
                                   stretch_specification)
                      .AddChildren(views::Builder<views::ImageView>().SetImage(
                                       extension_icon),
                                   views::Builder<views::Label>().SetText(
                                       extension_name)),
                  // Close button.
                  views::Builder<views::Button>(
                      views::BubbleFrameView::CreateCloseButton(
                          base::BindRepeating(
                              &ExtensionsMenuNavigationHandler::CloseBubble,
                              base::Unretained(navigation_handler))))),
          // Content.
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .AddChildren(
                  // Settings button.
                  views::Builder<views::Separator>(),
                  views::Builder<HoverButton>(std::make_unique<HoverButton>(
                      base::BindRepeating(
                          [](Browser* browser,
                             extensions::ExtensionId extension_id) {
                            chrome::ShowExtensions(browser, extension_id);
                          },
                          browser, extension_id),
                      /*icon_view=*/nullptr,
                      l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_SETTINGS_BUTTON),
                      /*subtitle=*/std::u16string(),
                      std::make_unique<views::ImageView>(
                          ui::ImageModel::FromVectorIcon(
                              vector_icons::kLaunchIcon,
                              ui::kColorIconSecondary))))))

      .BuildChildren();
}

// TODO(crbug.com/1390952): Update content once content is added to this page.

BEGIN_METADATA(ExtensionsMenuSitePermissionsPageView, views::View)
END_METADATA
