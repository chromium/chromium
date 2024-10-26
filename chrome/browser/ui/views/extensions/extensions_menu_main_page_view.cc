// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include <memory>
#include <string>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_handler.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "extensions/common/extension_urls.h"
#endif

namespace {

// Radius for the containers in the extensions menu.
constexpr int kContainerBackgroundRadius = 12;

// Index of the extension's request entry child view containing the icon in the
// requests container.
constexpr int kRequestEntryIconIndex = 0;
// Index of the extension's request entry child view containing the label in the
// requests container.
constexpr int kRequestEntryLabelIndex = 1;

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
  for (views::View* view : parent_view->children()) {
    auto* item_view = GetAsMenuItem(view);
    if (item_view->view_controller()->GetId() == action_id) {
      return item_view;
    }
  }
  return nullptr;
}

}  // namespace

// Base class for a container inside the extensions menu.
class SectionContainer : public views::BoxLayoutView {
 public:
  SectionContainer() {
    auto* layout_provider = ChromeLayoutProvider::Get();
    const int vertical_margin = layout_provider->GetDistanceMetric(
        DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE);
    const int horizontal_margin = layout_provider->GetDistanceMetric(
        DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE);

    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetInsideBorderInsets(gfx::Insets::VH(vertical_margin, horizontal_margin));
    SetBackground(views::CreateThemedRoundedRectBackground(
        kColorExtensionsMenuContainerBackground, kContainerBackgroundRadius));
  }
  SectionContainer(const SectionContainer&) = delete;
  const SectionContainer& operator=(const SectionContainer&) = delete;
  ~SectionContainer() override = default;
};

BEGIN_VIEW_BUILDER(/* No Export */, SectionContainer, views::BoxLayoutView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* No Export */, SectionContainer)

ExtensionsMenuMainPageView::ExtensionsMenuMainPageView(
    Browser* browser,
    ExtensionsMenuHandler* menu_handler)
    : browser_(browser), menu_handler_(menu_handler) {
  // This is set so that the extensions menu doesn't fall outside the monitor in
  // a maximized window in 1024x768. See https://crbug.com/1096630.
  // TODO(crbug.com/40891805): Consider making the height dynamic.
  constexpr int kMaxExtensionButtonsHeightDp = 448;
  views::FlexSpecification stretch_specification =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1);

  ChromeLayoutProvider* const chrome_layout_provider =
      ChromeLayoutProvider::Get();
  const int control_vertical_spacing =
      chrome_layout_provider->GetDistanceMetric(
          DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  // This value must be the same as the `HoverButton` vertical margin.
  const int hover_button_vertical_spacing =
      chrome_layout_provider->GetDistanceMetric(
          DISTANCE_CONTROL_LIST_VERTICAL) /
      2;

  views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  const gfx::Insets dialog_insets =
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG);

  views::Builder<ExtensionsMenuMainPageView>(this)
      .SetProperty(views::kElementIdentifierKey,
                   kExtensionsMenuMainPageElementId)
      // Last item is a hover button, so we need to account for the extra
      // vertical spacing. We cannot add horizontal margins at this level
      // because some views need to expand the full length (e.g settings
      // button). Thus, each view needs to add the horizontal margins
      // accordingly.
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::TLBR(
              dialog_insets.top(), 0,
              dialog_insets.bottom() - hover_button_vertical_spacing, 0)))
      .AddChildren(
          // Header.
          views::Builder<views::FlexLayoutView>()
              .SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(0, dialog_insets.left()))
              .AddChildren(
                  // Title.
                  views::Builder<views::Label>()
                      .SetText(
                          l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_TITLE))
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                      .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
                      .SetTextStyle(views::style::STYLE_HEADLINE_4)
                      .SetEnabledColorId(kColorExtensionsMenuText)
                      .SetProperty(views::kFlexBehaviorKey,
                                   stretch_specification),
                  // Close button.
                  views::Builder<views::Button>(
                      views::BubbleFrameView::CreateCloseButton(
                          base::BindRepeating(
                              &ExtensionsMenuHandler::CloseBubble,
                              base::Unretained(menu_handler_))))),
          CreateSiteSettingsBuilder(
              /*margins=*/gfx::Insets::VH(control_vertical_spacing,
                                          dialog_insets.left()),
              stretch_specification, menu_handler),
          // Contents.
          views::Builder<views::ScrollView>()
              .ClipHeightTo(0, kMaxExtensionButtonsHeightDp)
              .SetDrawOverflowIndicator(false)
              .SetHorizontalScrollBarMode(
                  views::ScrollView::ScrollBarMode::kDisabled)
              .SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(control_vertical_spacing, 0))
              .SetContents(
                  views::Builder<views::BoxLayoutView>()
                      .SetOrientation(views::BoxLayout::Orientation::kVertical)
                      // Horizontal dialog margins are added inside the scroll
                      // view contents to have the scroll bar by the dialog
                      // border.
                      .SetInsideBorderInsets(
                          gfx::Insets::VH(0, dialog_insets.left()))
                      .AddChildren(
                          // Reload section.
                          views::Builder<SectionContainer>()
                              .CopyAddressTo(&reload_section_)
                              .SetVisible(false)
                              .SetCrossAxisAlignment(
                                  views::BoxLayout::CrossAxisAlignment::kCenter)
                              .AddChildren(
                                  views::Builder<views::Label>()
                                      .SetText(l10n_util::GetStringUTF16(
                                          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RELOAD_CONTAINER_DESCRIPTION_TEXT))
                                      .SetTextContext(
                                          ChromeTextContext::
                                              CONTEXT_DIALOG_BODY_TEXT_SMALL)
                                      .SetTextStyle(views::style::STYLE_BODY_3)
                                      .SetEnabledColorId(
                                          kColorExtensionsMenuSecondaryText)
                                      .SetMultiLine(true),
                                  views::Builder<views::MdTextButton>()
                                      .SetCallback(base::BindRepeating(
                                          &ExtensionsMenuHandler::
                                              OnReloadPageButtonClicked,
                                          base::Unretained(menu_handler_)))
                                      .SetBgColorIdOverride(
                                          kColorExtensionsMenuContainerBackground)
                                      .SetProperty(views::kMarginsKey,
                                                   gfx::Insets::TLBR(
                                                       control_vertical_spacing,
                                                       0, 0, 0))
                                      .SetText(l10n_util::GetStringUTF16(
                                          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RELOAD_CONTAINER_BUTTON_TEXT))
                                      .SetTooltipText(l10n_util::GetStringUTF16(
                                          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RELOAD_CONTAINER_BUTTON_TOOLTIP))),
                          // Extensions requests section.
                          views::Builder<SectionContainer>()
                              .CopyAddressTo(&requests_section_)
                              .SetVisible(false)
                              .AddChildren(
                                  // Header.
                                  views::Builder<views::Label>()
                                      .SetText(l10n_util::GetStringUTF16(
                                          IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_TITLE))
                                      .SetTextContext(
                                          ChromeTextContext::
                                              CONTEXT_DIALOG_BODY_TEXT_SMALL)
                                      .SetEnabledColorId(
                                          kColorExtensionsMenuText)
                                      .SetTextStyle(
                                          views::style::STYLE_BODY_2_EMPHASIS)
                                      .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                                  // Empty container for the requests entries.
                                  views::Builder<views::BoxLayoutView>()
                                      .CopyAddressTo(&requests_entries_view_)
                                      .SetOrientation(
                                          views::BoxLayout::Orientation::
                                              kVertical)),
                          // Menu items section.
                          views::Builder<SectionContainer>()
                              .CopyAddressTo(&menu_items_)
                              .SetProperty(
                                  views::kMarginsKey,
                                  gfx::Insets::TLBR(control_vertical_spacing, 0,
                                                    0, 0)))),
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          // Webstore button.
          views::Builder<HoverButton>(std::make_unique<HoverButton>(
              base::BindRepeating(&chrome::ShowWebStore, browser_,
                                  extension_urls::kExtensionsMenuUtmSource),
              ui::ImageModel::FromVectorIcon(
                  vector_icons::kGoogleChromeWebstoreIcon),
              l10n_util::GetStringUTF16(
                  IDS_EXTENSIONS_MENU_MAIN_PAGE_DISCOVER_EXTENSIONS))),
#endif
          // Manage extensions button.
          views::Builder<HoverButton>(
              std::make_unique<HoverButton>(
                  base::BindRepeating(
                      [](Browser* browser) {
                        base::RecordAction(base::UserMetricsAction(
                            "Extensions.Menu."
                            "ExtensionsSettingsOpened"));
                        chrome::ShowExtensions(browser);
                      },
                      browser_),
                  ui::ImageModel::FromVectorIcon(
                      vector_icons::kSettingsChromeRefreshIcon),
                  l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS)))
              .SetProperty(views::kElementIdentifierKey,
                           kExtensionsMenuManageExtensionsElementId))
      .BuildChildren();

  // By default, the button's accessible description is set to the button's
  // tooltip text. This is the accepted workaround to ensure only accessible
  // name is announced by a screenreader rather than tooltip text and
  // accessible name.
  site_settings_toggle_->GetViewAccessibility().SetDescription(
      std::u16string(), ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  site_settings_toggle_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_ACCESSIBLE_NAME));

  // Align the site setting toggle vertically with the site settings label by
  // getting the label height after construction.
  site_settings_toggle_->SetPreferredSize(
      gfx::Size(site_settings_toggle_->GetPreferredSize().width(),
                site_settings_label_->GetLineHeight()));
}

ExtensionsMenuMainPageView::~ExtensionsMenuMainPageView() = default;

void ExtensionsMenuMainPageView::CreateAndInsertMenuItem(
    std::unique_ptr<ExtensionActionViewController> action_controller,
    extensions::ExtensionId extension_id,
    bool is_enterprise,
    ExtensionMenuItemView::SiteAccessToggleState site_access_toggle_state,
    ExtensionMenuItemView::SitePermissionsButtonState
        site_permissions_button_state,
    ExtensionMenuItemView::SitePermissionsButtonAccess
        site_permissions_button_access,
    int index) {
  // base::Unretained() below is safe because `menu_handler_` lifetime is
  // tied to this view lifetime by the extensions menu coordinator.
  auto item = std::make_unique<ExtensionMenuItemView>(
      browser_, is_enterprise, std::move(action_controller),
      base::BindRepeating(&ExtensionsMenuHandler::OnExtensionToggleSelected,
                          base::Unretained(menu_handler_), extension_id),
      base::BindRepeating(&ExtensionsMenuHandler::OpenSitePermissionsPage,
                          base::Unretained(menu_handler_), extension_id));
  item->Update(site_access_toggle_state, site_permissions_button_state,
               site_permissions_button_access, is_enterprise);

  // Add vertical spacing in between menu items.
  if (index > 0) {
    ChromeLayoutProvider* const chrome_layout_provider =
        ChromeLayoutProvider::Get();
    const int control_vertical_spacing =
        chrome_layout_provider->GetDistanceMetric(
            DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
    item->SetInteriorMargin(
        gfx::Insets::TLBR(control_vertical_spacing, 0, 0, 0));
  }

  menu_items_->AddChildViewAt(std::move(item), index);
}

void ExtensionsMenuMainPageView::RemoveMenuItem(
    const ToolbarActionsModel::ActionId& action_id) {
  views::View* item = GetMenuItem(menu_items_, action_id);
  menu_items_->RemoveChildViewT(item);
}

void ExtensionsMenuMainPageView::UpdateSiteSettings(
    const std::u16string& current_site,
    int label_id,
    bool is_tooltip_visible,
    bool is_toggle_visible,
    bool is_toggle_on) {
  site_settings_label_->SetText(
      l10n_util::GetStringFUTF16(label_id, current_site));
  site_settings_tooltip_->SetVisible(is_tooltip_visible);
  site_settings_toggle_->SetVisible(is_toggle_visible);
  site_settings_toggle_->SetIsOn(is_toggle_on);
  site_settings_toggle_->SetTooltipText(GetSiteSettingToggleText(is_toggle_on));
}

void ExtensionsMenuMainPageView::ShowReloadSection() {
  reload_section_->SetVisible(true);
  requests_section_->SetVisible(false);
  SizeToPreferredSize();
}

void ExtensionsMenuMainPageView::MaybeShowRequestsSection() {
  reload_section_->SetVisible(false);
  requests_section_->SetVisible(!requests_entries_.empty());
  SizeToPreferredSize();
}

void ExtensionsMenuMainPageView::AddOrUpdateExtensionRequestingAccess(
    const extensions::ExtensionId& id,
    const std::u16string& name,
    const ui::ImageModel& icon,
    int index) {
  // Update request entry if existent.
  views::View* request_entry = GetExtensionRequestEntry(id);
  if (request_entry) {
    std::vector<raw_ptr<View, VectorExperimental>> extension_items =
        request_entry->children();
    views::AsViewClass<views::ImageView>(
        extension_items[kRequestEntryIconIndex])
        ->SetImage(icon);
    views::AsViewClass<views::Label>(extension_items[kRequestEntryLabelIndex])
        ->SetText(name);
    requests_entries_view_->ReorderChildView(request_entry, index);
  }

  // Otherwise, add a new request entry.
  auto* layout_provider = ChromeLayoutProvider::Get();
  const int control_vertical_margin = layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  const int related_control_horizontal_margin =
      layout_provider->GetDistanceMetric(
          DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);

  auto item =
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(control_vertical_margin, 0, 0, 0))
          .AddChildren(
              views::Builder<views::ImageView>().SetImage(icon),
              views::Builder<views::Label>()
                  .SetText(name)
                  .SetTextStyle(views::style::STYLE_BODY_3_EMPHASIS)
                  .SetEnabledColorId(kColorExtensionsMenuText)
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded)),
              views::Builder<views::MdTextButton>()
                  .SetCallback(base::BindRepeating(
                      &ExtensionsMenuHandler::OnDismissExtensionClicked,
                      base::Unretained(menu_handler_), id))
                  .SetStyle(ui::ButtonStyle::kText)
                  .SetBgColorIdOverride(kColorExtensionsMenuContainerBackground)
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_DISMISS_BUTTON_TEXT))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_DISMISS_BUTTON_TOOLTIP)),
              views::Builder<views::MdTextButton>()
                  .SetCallback(base::BindRepeating(
                      &ExtensionsMenuHandler::OnAllowExtensionClicked,
                      base::Unretained(menu_handler_), id))
                  .SetStyle(ui::ButtonStyle::kText)
                  .SetBgColorIdOverride(kColorExtensionsMenuContainerBackground)
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_ALLOW_BUTTON_TEXT))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_ALLOW_BUTTON_TOOLTIP))
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets::TLBR(
                                   0, related_control_horizontal_margin, 0, 0)))
          .Build();

  requests_entries_.insert({id, item.get()});
  requests_entries_view_->AddChildViewAt(std::move(item), index);
}

void ExtensionsMenuMainPageView::RemoveExtensionRequestingAccess(
    const extensions::ExtensionId& id) {
  views::View* request_entry = GetExtensionRequestEntry(id);
  if (!request_entry) {
    return;
  }

  requests_entries_view_->RemoveChildViewT(request_entry);
  requests_entries_.erase(id);
}

void ExtensionsMenuMainPageView::ClearExtensionsRequestingAccess() {
  requests_entries_view_->RemoveAllChildViews();
  requests_entries_.clear();

  requests_section_->SetVisible(false);
  SizeToPreferredSize();
}

std::vector<ExtensionMenuItemView*> ExtensionsMenuMainPageView::GetMenuItems()
    const {
  std::vector<ExtensionMenuItemView*> menu_item_views;
  for (views::View* view : menu_items_->children()) {
    menu_item_views.push_back(GetAsMenuItem(view));
  }
  return menu_item_views;
}

const std::u16string&
ExtensionsMenuMainPageView::GetSiteSettingLabelForTesting() const {
  CHECK_IS_TEST();
  return site_settings_label_->GetText();
}

const views::View* ExtensionsMenuMainPageView::site_settings_tooltip() const {
  CHECK_IS_TEST();
  return site_settings_tooltip_;
}

const views::View* ExtensionsMenuMainPageView::reload_section() const {
  CHECK_IS_TEST();
  return reload_section_;
}

const views::View* ExtensionsMenuMainPageView::requests_section() const {
  CHECK_IS_TEST();
  return requests_section_;
}

std::vector<extensions::ExtensionId>
ExtensionsMenuMainPageView::GetExtensionsRequestingAccessForTesting() {
  CHECK_IS_TEST();
  std::vector<extensions::ExtensionId> extensions;
  extensions.reserve(requests_entries_.size());
  for (auto entry : requests_entries_) {
    extensions.push_back(entry.first);
  }
  return extensions;
}

views::View*
ExtensionsMenuMainPageView::GetExtensionRequestingAccessEntryForTesting(
    const extensions::ExtensionId& extension_id) {
  CHECK_IS_TEST();
  return GetExtensionRequestEntry(extension_id);
}

content::WebContents* ExtensionsMenuMainPageView::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

views::View* ExtensionsMenuMainPageView::GetExtensionRequestEntry(
    const extensions::ExtensionId& extension_id) const {
  auto iter = requests_entries_.find(extension_id);
  return iter == requests_entries_.end() ? nullptr : iter->second;
}

views::Builder<views::FlexLayoutView>
ExtensionsMenuMainPageView::CreateSiteSettingsBuilder(
    gfx::Insets margins,
    views::FlexSpecification stretch_specification,
    ExtensionsMenuHandler* menu_handler) {
  views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  int tooltip_bubble_width = layout_provider->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);

  return views::Builder<views::FlexLayoutView>()
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetProperty(views::kMarginsKey, margins)
      .AddChildren(
          views::Builder<views::FlexLayoutView>()
              .SetProperty(views::kFlexBehaviorKey, stretch_specification)
              .AddChildren(
                  views::Builder<views::Label>()
                      .CopyAddressTo(&site_settings_label_)
                      .SetTextStyle(views::style::STYLE_BODY_3_EMPHASIS)
                      .SetEnabledColorId(kColorExtensionsMenuText)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                  views::Builder<views::TooltipIcon>(
                      std::make_unique<
                          views::TooltipIcon>(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_ENTERPRISE_TOOLTIP_ICON_TEXT)))
                      .CopyAddressTo(&site_settings_tooltip_)
                      .SetBubbleWidth(tooltip_bubble_width)
                      .SetAnchorPointArrow(
                          views::BubbleBorder::Arrow::TOP_RIGHT)),
          views::Builder<views::ToggleButton>()
              .CopyAddressTo(&site_settings_toggle_)
              .SetCallback(base::BindRepeating(
                  [](views::ToggleButton* toggle_button,
                     base::RepeatingCallback<void(bool)> callback) {
                    callback.Run(toggle_button->GetIsOn());
                  },
                  site_settings_toggle_,
                  base::BindRepeating(
                      &ExtensionsMenuHandler::OnSiteSettingsToggleButtonPressed,
                      base::Unretained(menu_handler)))));
}

BEGIN_METADATA(ExtensionsMenuMainPageView)
END_METADATA
