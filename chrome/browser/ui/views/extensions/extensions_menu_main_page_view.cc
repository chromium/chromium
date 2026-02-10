// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_entry_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/extension_id.h"
#include "ui/base/class_property.h"
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
#include "ui/views/metadata/view_factory.h"
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

// Converts a view to a ExtensionsMenuEntryView. This cannot be used to
// *determine* if a view is an ExtensionsMenuEntryView (it should only be used
// when the view is known to be one). It is only used as an extra measure to
// prevent bad static casts.
ExtensionsMenuEntryView* GetAsMenuEntry(views::View* view) {
  DCHECK(views::IsViewClass<ExtensionsMenuEntryView>(view));
  return views::AsViewClass<ExtensionsMenuEntryView>(view);
}

}  // namespace

// A view property key used to store the extension ID on the "requests access"
// menu entries. This allows identifying the specific extension associated with
// a view, primarily for testing purposes.
struct ExtensionIdWrapper {
  std::string id;
};
DEFINE_UI_CLASS_PROPERTY_TYPE(ExtensionIdWrapper*)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ExtensionIdWrapper, kExtensionIdKey)

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
    SetBackground(views::CreateRoundedRectBackground(
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
          views::DISTANCE_CONTROL_LIST_VERTICAL) /
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
          CreateHeaderBuilder(
              /*margins=*/gfx::Insets::VH(0, dialog_insets.left()),
              stretch_specification),
          CreateSiteSettingsBuilder(
              /*margins=*/gfx::Insets::VH(control_vertical_spacing,
                                          dialog_insets.left()),
              stretch_specification),
          CreateContentsBuilder(
              /*scroll_margins=*/gfx::Insets::VH(control_vertical_spacing, 0),
              /*contents_margins=*/gfx::Insets::VH(0, dialog_insets.left()),
              /*reload_button_margins=*/
              gfx::Insets::TLBR(control_vertical_spacing, 0, 0, 0),
              /*menu_entries_margins=*/
              gfx::Insets::TLBR(control_vertical_spacing, 0, 0, 0)),
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          CreateWebstoreButtonBuilder(),
#endif
          CreateManageButtonBuilder())
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

void ExtensionsMenuMainPageView::CreateAndInsertMenuEntry(
    ExtensionActionViewModel* action_model,
    ExtensionsMenuViewModel::MenuEntryState entry_state,
    int index) {
  auto extension_id = action_model->GetId();
  // base::Unretained() below is safe because `menu_handler_` lifetime is
  // tied to this view lifetime by the extensions menu coordinator.
  auto item = std::make_unique<ExtensionsMenuEntryView>(
      browser_, entry_state.is_enterprise, action_model,
      base::BindRepeating(&ExtensionsMenuHandler::OnActionButtonClicked,
                          base::Unretained(menu_handler_), extension_id),
      base::BindRepeating(&ExtensionsMenuHandler::OnExtensionToggleSelected,
                          base::Unretained(menu_handler_), extension_id),
      base::BindRepeating(&ExtensionsMenuHandler::OpenSitePermissionsPage,
                          base::Unretained(menu_handler_), extension_id));
  item->Update(entry_state);

  // Add vertical spacing in between menu entries.
  if (index > 0) {
    ChromeLayoutProvider* const chrome_layout_provider =
        ChromeLayoutProvider::Get();
    const int control_vertical_spacing =
        chrome_layout_provider->GetDistanceMetric(
            DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
    item->SetInteriorMargin(
        gfx::Insets::TLBR(control_vertical_spacing, 0, 0, 0));
  }

  menu_entries_->AddChildViewAt(std::move(item), index);
}

void ExtensionsMenuMainPageView::RemoveMenuEntry(int index) {
  menu_entries_->RemoveChildViewT(menu_entries_->children().at(index));
}

void ExtensionsMenuMainPageView::UpdateSiteSettings(
    ExtensionsMenuViewModel::SiteSettingsState site_settings_state) {
  site_settings_label_->SetText(site_settings_state.label);
  site_settings_tooltip_->SetVisible(site_settings_state.has_tooltip);
  site_settings_toggle_->SetVisible(
      site_settings_state.toggle.status !=
      ExtensionsMenuViewModel::ControlState::Status::kHidden);
  site_settings_toggle_->SetIsOn(site_settings_state.toggle.is_on);
  site_settings_toggle_->SetTooltipText(
      site_settings_state.toggle.tooltip_text);
}

void ExtensionsMenuMainPageView::AddExtensionRequestingAccess(
    ExtensionsMenuViewModel::HostAccessRequest request,
    int index) {
  auto* layout_provider = ChromeLayoutProvider::Get();
  const int control_vertical_margin = layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  const int related_control_horizontal_margin =
      layout_provider->GetDistanceMetric(
          DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);

  auto item =
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetProperty(kExtensionIdKey,
                       ExtensionIdWrapper{request.extension_id})
          .SetProperty(views::kMarginsKey,
                       gfx::Insets::TLBR(control_vertical_margin, 0, 0, 0))
          .AddChildren(
              views::Builder<views::ImageView>().SetImage(
                  request.extension_icon),
              views::Builder<views::Label>()
                  .SetText(request.extension_name)
                  .SetTextStyle(views::style::STYLE_BODY_3_EMPHASIS)
                  .SetEnabledColor(kColorExtensionsMenuText)
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::LayoutOrientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded)),
              views::Builder<views::MdTextButton>()
                  .SetCallback(base::BindRepeating(
                      &ExtensionsMenuHandler::OnDismissExtensionClicked,
                      base::Unretained(menu_handler_), request.extension_id))
                  .SetStyle(ui::ButtonStyle::kText)
                  .SetBgColorIdOverride(kColorExtensionsMenuContainerBackground)
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_DISMISS_BUTTON_TEXT))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_DISMISS_BUTTON_TOOLTIP))
                  .SetAccessibleName(l10n_util::GetStringFUTF16(
                      IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_DISMISS_BUTTON_ACCESSIBLE_NAME,
                      request.extension_name)),
              views::Builder<views::MdTextButton>()
                  .SetCallback(base::BindRepeating(
                      &ExtensionsMenuHandler::OnAllowExtensionClicked,
                      base::Unretained(menu_handler_), request.extension_id))
                  .SetStyle(ui::ButtonStyle::kText)
                  .SetBgColorIdOverride(kColorExtensionsMenuContainerBackground)
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_ALLOW_BUTTON_TEXT))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_ALLOW_BUTTON_TOOLTIP))
                  .SetAccessibleName(l10n_util::GetStringFUTF16(
                      IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_ALLOW_BUTTON_ACCESSIBLE_NAME,
                      request.extension_name))
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets::TLBR(
                                   0, related_control_horizontal_margin, 0, 0)))
          .Build();

  requests_entries_view_->AddChildViewAt(std::move(item), index);
}

void ExtensionsMenuMainPageView::UpdateExtensionRequestingAccess(
    ExtensionsMenuViewModel::HostAccessRequest request,
    int index) {
  // Verify the index is valid for the current layout.
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index),
           requests_entries_view_->children().size());

  views::View* request_view = requests_entries_view_->children().at(index);
  CHECK(request_view);

  std::vector<raw_ptr<View, VectorExperimental>> extension_items =
      request_view->children();
  views::AsViewClass<views::ImageView>(extension_items[kRequestEntryIconIndex])
      ->SetImage(request.extension_icon);
  views::AsViewClass<views::Label>(extension_items[kRequestEntryLabelIndex])
      ->SetText(request.extension_name);
  requests_entries_view_->ReorderChildView(request_view, index);
}

void ExtensionsMenuMainPageView::RemoveExtensionRequestingAccess(
    const extensions::ExtensionId& id,
    int index) {
  // Verify the index is valid for the current layout.
  CHECK_GE(index, 0);
  CHECK_LT(static_cast<size_t>(index),
           requests_entries_view_->children().size());

  views::View* request_view = requests_entries_view_->children().at(index);
  requests_entries_view_->RemoveChildViewT(request_view);
}

void ExtensionsMenuMainPageView::ClearExtensionsRequestingAccess() {
  requests_entries_view_->RemoveAllChildViews();
}

void ExtensionsMenuMainPageView::SetOptionalSectionVisibility(
    ExtensionsMenuViewModel::OptionalSection optional_section) {
  switch (optional_section) {
    case ExtensionsMenuViewModel::OptionalSection::kReloadPage:
      reload_section_->SetVisible(true);
      requests_section_->SetVisible(false);
      break;
    case ExtensionsMenuViewModel::OptionalSection::kHostAccessRequests:
      reload_section_->SetVisible(false);
      requests_section_->SetVisible(
          !requests_entries_view_->children().empty());
      break;
    case ExtensionsMenuViewModel::OptionalSection::kNone:
      reload_section_->SetVisible(false);
      requests_section_->SetVisible(false);
      break;
  }

  SizeToPreferredSize();
}

std::vector<ExtensionsMenuEntryView*>
ExtensionsMenuMainPageView::GetMenuEntries() const {
  std::vector<ExtensionsMenuEntryView*> menu_entry_views;
  for (views::View* view : menu_entries_->children()) {
    menu_entry_views.push_back(GetAsMenuEntry(view));
  }
  return menu_entry_views;
}

std::u16string_view ExtensionsMenuMainPageView::GetSiteSettingLabelForTesting()
    const {
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
  extensions.reserve(requests_entries_view_->children().size());

  for (views::View* view : requests_entries_view_->children()) {
    const ExtensionIdWrapper* id_wrapper = view->GetProperty(kExtensionIdKey);
    CHECK(id_wrapper);
    extensions.push_back(id_wrapper->id);
  }
  return extensions;
}

views::View*
ExtensionsMenuMainPageView::GetExtensionRequestingAccessEntryForTesting(
    const extensions::ExtensionId& extension_id) {
  CHECK_IS_TEST();

  for (views::View* view : requests_entries_view_->children()) {
    const ExtensionIdWrapper* id_wrapper = view->GetProperty(kExtensionIdKey);
    if (id_wrapper && id_wrapper->id == extension_id) {
      return view;
    }
  }
  return nullptr;
}

views::Builder<views::FlexLayoutView>
ExtensionsMenuMainPageView::CreateHeaderBuilder(
    gfx::Insets margins,
    views::FlexSpecification stretch_specification) {
  return views::Builder<views::FlexLayoutView>()
      .SetProperty(views::kMarginsKey, margins)
      .AddChildren(
          // Title.
          views::Builder<views::Label>()
              .SetText(l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_TITLE))
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
              .SetTextStyle(views::style::STYLE_HEADLINE_4)
              .SetEnabledColor(kColorExtensionsMenuText)
              .SetProperty(views::kFlexBehaviorKey, stretch_specification),
          // Close button.
          views::Builder<views::Button>(
              views::BubbleFrameView::CreateCloseButton(
                  base::BindRepeating(&ExtensionsMenuHandler::CloseBubble,
                                      base::Unretained(menu_handler_)))));
}

views::Builder<views::FlexLayoutView>
ExtensionsMenuMainPageView::CreateSiteSettingsBuilder(
    gfx::Insets margins,
    views::FlexSpecification stretch_specification) {
  views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  int tooltip_bubble_width = layout_provider->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  int menu_button_margin = layout_provider->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN);

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
                      .SetMultiLine(true)
                      .SetAllowCharacterBreak(true)
                      .SetEnabledColor(kColorExtensionsMenuText)
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
              .SetProperty(views::kElementIdentifierKey,
                           kExtensionsMenuSiteSettingsToggleElementId)
              .SetProperty(views::kMarginsKey,
                           gfx::Insets::TLBR(0, menu_button_margin, 0, 0))
              .SetCallback(base::BindRepeating(
                  [](views::ToggleButton* toggle_button,
                     base::RepeatingCallback<void(bool)> callback) {
                    callback.Run(toggle_button->GetIsOn());
                  },
                  site_settings_toggle_,
                  base::BindRepeating(
                      &ExtensionsMenuHandler::OnSiteSettingsToggleButtonPressed,
                      base::Unretained(menu_handler_)))));
}

views::Builder<views::ScrollView>
ExtensionsMenuMainPageView::CreateContentsBuilder(
    gfx::Insets scroll_margins,
    gfx::Insets contents_margins,
    gfx::Insets reload_button_margins,
    gfx::Insets menu_entries_margins) {
  // This is set so that the extensions menu doesn't fall outside the monitor in
  // a maximized window in 1024x768. See https://crbug.com/1096630.
  // TODO(crbug.com/40891805): Consider making the height dynamic.
  constexpr int kMaxExtensionButtonsHeightDp = 448;

  return views::Builder<views::ScrollView>()
      .ClipHeightTo(0, kMaxExtensionButtonsHeightDp)
      .SetDrawOverflowIndicator(false)
      .SetHorizontalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled)
      .SetProperty(views::kMarginsKey, scroll_margins)
      .SetContents(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              // Horizontal dialog margins are added inside the scroll
              // view contents to have the scroll bar by the dialog
              // border.
              .SetInsideBorderInsets(contents_margins)
              .AddChildren(
                  // Reload section.
                  views::Builder<SectionContainer>()
                      .CopyAddressTo(&reload_section_)
                      .SetProperty(views::kElementIdentifierKey,
                                   kExtensionsMenuReloadSectionElementId)
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
                              .SetEnabledColor(
                                  kColorExtensionsMenuSecondaryText)
                              .SetMultiLine(true),
                          views::Builder<views::MdTextButton>()
                              .SetProperty(
                                  views::kElementIdentifierKey,
                                  kExtensionsMenuReloadPageButtonElementId)
                              .SetCallback(base::BindRepeating(
                                  &ExtensionsMenuHandler::
                                      OnReloadPageButtonClicked,
                                  base::Unretained(menu_handler_)))
                              .SetBgColorIdOverride(
                                  kColorExtensionsMenuContainerBackground)
                              .SetProperty(views::kMarginsKey,
                                           reload_button_margins)
                              .SetText(l10n_util::GetStringUTF16(
                                  IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RELOAD_CONTAINER_BUTTON_TEXT))
                              .SetTooltipText(l10n_util::GetStringUTF16(
                                  IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RELOAD_CONTAINER_BUTTON_TOOLTIP))),
                  // Access requests section.
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
                              .SetEnabledColor(kColorExtensionsMenuText)
                              .SetTextStyle(views::style::STYLE_BODY_2_EMPHASIS)
                              .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                          // Empty container for the requests entries.
                          views::Builder<views::BoxLayoutView>()
                              .CopyAddressTo(&requests_entries_view_)
                              .SetOrientation(
                                  views::BoxLayout::Orientation::kVertical)),
                  // menu entries section.
                  views::Builder<SectionContainer>()
                      .CopyAddressTo(&menu_entries_)
                      .SetProperty(views::kMarginsKey, menu_entries_margins)));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
views::Builder<HoverButton>
ExtensionsMenuMainPageView::CreateWebstoreButtonBuilder() {
  return views::Builder<HoverButton>(std::make_unique<HoverButton>(
      base::BindRepeating(&chrome::ShowWebStore, browser_,
                          extension_urls::kExtensionsMenuUtmSource),
      ui::ImageModel::FromVectorIcon(vector_icons::kGoogleChromeWebstoreIcon),
      l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MAIN_PAGE_DISCOVER_EXTENSIONS)));
}
#endif

views::Builder<HoverButton>
ExtensionsMenuMainPageView::CreateManageButtonBuilder() {
  return views::Builder<HoverButton>(
             std::make_unique<HoverButton>(
                 base::BindRepeating(
                     [](Browser* browser) {
                       base::RecordAction(
                           base::UserMetricsAction("Extensions.Menu."
                                                   "ExtensionsSettingsOpened"));
                       chrome::ShowExtensions(browser);
                     },
                     browser_),
                 ui::ImageModel::FromVectorIcon(
                     vector_icons::kSettingsChromeRefreshIcon),
                 l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS)))
      .SetProperty(views::kElementIdentifierKey,
                   kExtensionsMenuManageExtensionsElementId);
}

BEGIN_METADATA(ExtensionsMenuMainPageView) END_METADATA
