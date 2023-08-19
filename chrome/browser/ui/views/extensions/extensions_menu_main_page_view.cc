// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_handler.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
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

// View that contains a special message inside the extensions menu main page
// depending on its state.
class MessageSection : public views::BoxLayoutView {
 public:
  // The type of view container in the message section. At most one of this is
  // visible at all times.
  enum class ContainerType {
    // Container with a label.
    kTextContainer,
    // Container with labels and reload button.
    kReloadContainer,
    // Container with extensions requesting access.
    kRequestsAccessContainer
  };

  MessageSection(base::RepeatingCallback<void()> reload_callback,
                 base::RepeatingCallback<void(const extensions::ExtensionId&)>
                     allow_callback,
                 base::RepeatingCallback<void(const extensions::ExtensionId&)>
                     dismiss_callback);
  MessageSection(const MessageSection&) = delete;
  const MessageSection& operator=(const MessageSection&) = delete;
  ~MessageSection() override = default;

  // Updates the views contents and visibility given `state` and
  // `has_enterprise_extensions`.
  void Update(ExtensionsMenuMainPageView::MessageSectionState state,
              bool has_enterprise_extensions);

  // Adds an entry in `extensions_container_` for the extension with `id`,
  // `name` and `icon` at `index`. If the extension is already present, it
  // updates the entry. Shows the sections if it's the first extension entry.
  // Note that `state_` must be `kUserCustomizedAccess`.
  void AddOrUpdateExtension(const extensions::ExtensionId& id,
                            const std::u16string& name,
                            const ui::ImageModel& icon,
                            int index);

  // Removes the entry corresponding to `id`, if existent. Hides the section if
  // no extension entries are remaining. Note that `state_` must be
  // `kUserCustomizedAccess`.
  void RemoveExtension(const extensions::ExtensionId& id);

  // Accessors used by tests:
  views::View* GetTextContainerForTesting() { return text_container_; }
  views::View* GetReloadContainerForTesting() { return reload_container_; }
  views::View* GetRequestsAccessContainerForTesting() {
    return requests_access_container_;
  }
  std::vector<extensions::ExtensionId> GetExtensionsForTesting();
  views::View* GetExtensionEntryForTesting(
      const extensions::ExtensionId& extension_id);

 private:
  static constexpr int kTextContainerTextIndex = 0;
  static constexpr int kTextContainerTooltipIconIndex = 1;
  static constexpr int kReloadContainerMainTextIndex = 0;
  static constexpr int kExtensionItemsContainerIndex = 1;
  static constexpr int kExtensionItemIconIndex = 0;
  static constexpr int kExtensionItemLabelIndex = 1;

  // Removes all extension entries.
  void ClearExtensions();

  // Updates the visibility of the view based on `state_` and
  // `extension_entries_`.
  void UpdateVisibility();

  // Updates the containers visibility and content given a `container_type`,
  // `label_id` and `show_label_tooltip`. At most only one of the "containers"
  // will be visible.
  void UpdateContainer(ContainerType container_type,
                       int label_id = -1,
                       bool show_label_tooltip = false);

  // The current state of the section.
  ExtensionsMenuMainPageView::MessageSectionState state_;

  // Text container.
  raw_ptr<views::View> text_container_;

  // Reload container.
  raw_ptr<views::View> reload_container_;
  // Callback for the reload button in `reload_container_`.
  base::RepeatingCallback<void()> reload_callback_;

  // Request access container
  raw_ptr<views::View> requests_access_container_;
  // A collection of all the extension entries in the request access container.
  std::map<extensions::ExtensionId, views::View*> extension_entries_;

  // Callback for the buttons in the extension entries.
  base::RepeatingCallback<void(const extensions::ExtensionId&)> allow_callback_;
  base::RepeatingCallback<void(const extensions::ExtensionId&)>
      dismiss_callback_;
};

BEGIN_VIEW_BUILDER(/* No Export */, MessageSection, views::BoxLayoutView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* No Export */, MessageSection)

MessageSection::MessageSection(
    base::RepeatingCallback<void()> reload_callback,
    base::RepeatingCallback<void(const extensions::ExtensionId&)>
        allow_callback,
    base::RepeatingCallback<void(const extensions::ExtensionId&)>
        dismiss_callback)
    : reload_callback_(std::move(reload_callback)),
      allow_callback_(std::move(allow_callback)),
      dismiss_callback_(std::move(dismiss_callback)) {
  auto* layout_provider = ChromeLayoutProvider::Get();
  const int section_vertical_margin = layout_provider->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE);
  const int section_horizontal_margin = layout_provider->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE);
  const int control_vertical_margin = layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);

  views::Builder<MessageSection>(this)
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      // TODO(crbug.com/1390952): After adding margins, compute radius from a
      // variable or create a const variable.
      .SetBackground(views::CreateThemedRoundedRectBackground(
          kColorExtensionsMenuHighlightedBackground, 4))
      .SetInsideBorderInsets(
          gfx::Insets::VH(section_vertical_margin, section_horizontal_margin))
      .AddChildren(
          // Text container.
          views::Builder<views::FlexLayoutView>()
              .CopyAddressTo(&text_container_)
              .SetVisible(false)
              .SetOrientation(views::LayoutOrientation::kHorizontal)
              .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
              .AddChildren(
                  // Main text.
                  views::Builder<views::Label>()
                      .SetTextContext(
                          ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
                      .SetHorizontalAlignment(gfx::ALIGN_CENTER),
                  // Enterprise info tooltip.
                  views::Builder<views::TooltipIcon>(
                      std::make_unique<
                          views::TooltipIcon>(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_ENTERPRISE_TOOLTIP_ICON_TEXT)))
                      .SetBubbleWidth(layout_provider->GetDistanceMetric(
                          views::DISTANCE_BUBBLE_PREFERRED_WIDTH))
                      .SetAnchorPointArrow(
                          views::BubbleBorder::Arrow::TOP_LEFT)),
          // Reload container.
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&reload_container_)
              .SetVisible(false)
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kCenter)
              .AddChildren(
                  // Main text.
                  views::Builder<views::Label>()
                      // Text will be set based on the `state_`.
                      .SetTextContext(
                          ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
                      .SetTextStyle(views::style::STYLE_EMPHASIZED),
                  // Description text.
                  views::Builder<views::Label>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RELOAD_CONTAINER_DESCRIPTION_TEXT))
                      .SetTextContext(
                          ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
                      .SetMultiLine(true),
                  // Reload button.
                  views::Builder<views::MdTextButton>()
                      .SetCallback(base::BindRepeating(reload_callback_))
                      .SetProperty(
                          views::kMarginsKey,
                          gfx::Insets::TLBR(control_vertical_margin, 0, 0, 0))
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RELOAD_CONTAINER_BUTTON_TEXT))
                      .SetTooltipText(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RELOAD_CONTAINER_BUTTON_TOOLTIP))),
          // Requests access container.
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&requests_access_container_)
              .SetVisible(false)
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .AddChildren(
                  // Header.
                  views::Builder<views::Label>()
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_TITLE))
                      .SetTextContext(
                          ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
                      .SetTextStyle(views::style::STYLE_EMPHASIZED)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                  // Empty container for the extensions requesting access.
                  views::Builder<views::BoxLayoutView>().SetOrientation(
                      views::BoxLayout::Orientation::kVertical)))
      .BuildChildren();
}

void MessageSection::Update(
    ExtensionsMenuMainPageView::MessageSectionState state,
    bool has_enterprise_extensions) {
  state_ = state;
  ContainerType container_type;
  int label_id;
  bool show_label_tooltip;

  switch (state_) {
    case ExtensionsMenuMainPageView::MessageSectionState::kRestrictedAccess:
      container_type = ContainerType::kTextContainer;
      label_id = IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RESTRICTED_ACCESS_TEXT;
      show_label_tooltip = false;
      break;
    case ExtensionsMenuMainPageView::MessageSectionState::kUserCustomizedAccess:
      container_type = ContainerType::kRequestsAccessContainer;
      // This state has a static label, thus we don't need to pass a label id.
      label_id = -1;
      show_label_tooltip = false;
      break;
    case ExtensionsMenuMainPageView::MessageSectionState::
        kUserCustomizedAccessReload:
      container_type = ContainerType::kReloadContainer;
      label_id =
          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_USER_CUSTOMIZED_ACCESS_TEXT;
      show_label_tooltip = false;
      break;
    case ExtensionsMenuMainPageView::MessageSectionState::kUserBlockedAccess:
      container_type = ContainerType::kTextContainer;
      label_id = IDS_EXTENSIONS_MENU_MESSAGE_SECTION_USER_BLOCKED_ACCESS_TEXT;
      // Tooltip can only be visible on this state, and if there are any
      // enterprise extensions installed.
      show_label_tooltip = has_enterprise_extensions;
      break;
    case ExtensionsMenuMainPageView::MessageSectionState::
        kUserBlockedAccessReload:
      container_type = ContainerType::kReloadContainer;
      label_id = IDS_EXTENSIONS_MENU_MESSAGE_SECTION_USER_BLOCKED_ACCESS_TEXT;
      show_label_tooltip = false;
      break;
  }

  UpdateContainer(container_type, label_id, show_label_tooltip);
  UpdateVisibility();
}

void MessageSection::UpdateContainer(ContainerType container_type,
                                     int label_id,
                                     bool show_label_tooltip) {
  switch (container_type) {
    case ContainerType::kTextContainer:
      DCHECK_NE(label_id, -1);
      text_container_->SetVisible(true);
      reload_container_->SetVisible(false);
      requests_access_container_->SetVisible(false);
      views::AsViewClass<views::Label>(
          text_container_->children()[kTextContainerTextIndex])
          ->SetText(l10n_util::GetStringUTF16(label_id));
      text_container_->children()[kTextContainerTooltipIconIndex]->SetVisible(
          show_label_tooltip);
      ClearExtensions();
      break;
    case ContainerType::kReloadContainer:
      DCHECK_NE(label_id, -1);
      text_container_->SetVisible(false);
      reload_container_->SetVisible(true);
      requests_access_container_->SetVisible(false);
      views::AsViewClass<views::Label>(
          reload_container_->children()[kReloadContainerMainTextIndex])
          ->SetText(l10n_util::GetStringUTF16(label_id));
      break;
    case ContainerType::kRequestsAccessContainer:
      DCHECK_EQ(label_id, -1);
      text_container_->SetVisible(false);
      reload_container_->SetVisible(false);
      requests_access_container_->SetVisible(!extension_entries_.empty());
      break;
  }
}

void MessageSection::AddOrUpdateExtension(const extensions::ExtensionId& id,
                                          const std::u16string& name,
                                          const ui::ImageModel& icon,
                                          int index) {
  CHECK_EQ(
      state_,
      ExtensionsMenuMainPageView::MessageSectionState::kUserCustomizedAccess);
  auto extension_iter = extension_entries_.find(id);

  if (extension_iter == extension_entries_.end()) {
    // Add new extension entry.
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
                    .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                    .SetProperty(views::kFlexBehaviorKey,
                                 views::FlexSpecification(
                                     views::MinimumFlexSizeRule::kScaleToZero,
                                     views::MaximumFlexSizeRule::kUnbounded)),
                views::Builder<views::MdTextButton>()
                    .SetCallback(base::BindRepeating(dismiss_callback_, id))
                    .SetText(l10n_util::GetStringUTF16(
                        IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_DISMISS_BUTTON_TEXT))
                    .SetTooltipText(l10n_util::GetStringUTF16(
                        IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_DISMISS_BUTTON_TOOLTIP)),
                views::Builder<views::MdTextButton>()
                    .SetCallback(base::BindRepeating(allow_callback_, id))
                    .SetText(l10n_util::GetStringUTF16(
                        IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_ALLOW_BUTTON_TEXT))
                    .SetTooltipText(l10n_util::GetStringUTF16(
                        IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_ALLOW_BUTTON_TOOLTIP))
                    .SetProperty(
                        views::kMarginsKey,
                        gfx::Insets::TLBR(0, related_control_horizontal_margin,
                                          0, 0)))
            .Build();
    extension_entries_.insert({id, item.get()});
    requests_access_container_->children()[1]->AddChildViewAt(std::move(item),
                                                              index);
    requests_access_container_->SetVisible(!extension_entries_.empty());
    UpdateVisibility();
  } else {
    // Update extension entry.
    std::vector<View*> extension_items = extension_iter->second->children();
    views::AsViewClass<views::ImageView>(
        extension_items[kExtensionItemIconIndex])
        ->SetImage(icon);
    views::AsViewClass<views::Label>(extension_items[kExtensionItemLabelIndex])
        ->SetText(name);
    requests_access_container_->children()[kExtensionItemsContainerIndex]
        ->ReorderChildView(extension_iter->second, index);
  }
}

void MessageSection::RemoveExtension(const extensions::ExtensionId& id) {
  CHECK_EQ(
      state_,
      ExtensionsMenuMainPageView::MessageSectionState::kUserCustomizedAccess);
  auto extension_iter = extension_entries_.find(id);
  if (extension_iter == extension_entries_.end()) {
    return;
  }

  requests_access_container_->children()[kExtensionItemsContainerIndex]
      ->RemoveChildViewT(extension_iter->second);
  extension_entries_.erase(extension_iter);

  requests_access_container_->SetVisible(!extension_entries_.empty());
  UpdateVisibility();
}

void MessageSection::ClearExtensions() {
  requests_access_container_->children()[kExtensionItemsContainerIndex]
      ->RemoveAllChildViews();
  extension_entries_.clear();
}

void MessageSection::UpdateVisibility() {
  // Section is always visible unless state is "user customized access" and no
  // extension is requesting site access.
  bool is_visible = state_ == ExtensionsMenuMainPageView::MessageSectionState::
                                  kUserCustomizedAccess
                        ? !extension_entries_.empty()
                        : true;
  SetVisible(is_visible);
}

std::vector<extensions::ExtensionId> MessageSection::GetExtensionsForTesting() {
  std::vector<extensions::ExtensionId> extensions;
  extensions.reserve(extension_entries_.size());
  for (auto entry : extension_entries_) {
    extensions.push_back(entry.first);
  }
  return extensions;
}

views::View* MessageSection::GetExtensionEntryForTesting(
    const extensions::ExtensionId& extension_id) {
  auto iter = extension_entries_.find(extension_id);
  return iter == extension_entries_.end() ? nullptr : iter->second;
}

ExtensionsMenuMainPageView::ExtensionsMenuMainPageView(
    Browser* browser,
    ExtensionsMenuHandler* menu_handler)
    : browser_(browser), menu_handler_(menu_handler) {
  // This is set so that the extensions menu doesn't fall outside the monitor in
  // a maximized window in 1024x768. See https://crbug.com/1096630.
  // TODO(crbug.com/1413883): Consider making the height dynamic.
  constexpr int kMaxExtensionButtonsHeightDp = 448;
  views::FlexSpecification stretch_specification =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1);

  ChromeLayoutProvider* const chrome_layout_provider =
      ChromeLayoutProvider::Get();
  const int vertical_spacing = chrome_layout_provider->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE);
  const int horizontal_spacing = chrome_layout_provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL);
  // This value must be the same as the `HoverButton` vertical margin.
  const int hover_button_vertical_spacing =
      chrome_layout_provider->GetDistanceMetric(
          DISTANCE_CONTROL_LIST_VERTICAL) /
      2;
  const int icon_size = chrome_layout_provider->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE);

  views::LayoutProvider* layout_provider = views::LayoutProvider::Get();
  const gfx::Insets dialog_insets =
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG);

  // Views that need configuration after construction (e.g access size after a
  // separate view is constructed).
  views::Label* subheader_title;

  views::Builder<ExtensionsMenuMainPageView>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      // TODO(crbug.com/1390952): Add margins after adding the menu
      // items, to make sure all items are aligned.
      .AddChildren(
          // Subheader section.
          views::Builder<views::FlexLayoutView>()
              .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
              // Add top dialog margins, since its the first element, and
              // horizontal dialog margins. Bottom margin will be added by the
              // next view (in general, vertical margins should be added by the
              // bottom view).
              .SetInteriorMargin(gfx::Insets::TLBR(dialog_insets.top(),
                                                   dialog_insets.left(), 0,
                                                   dialog_insets.right()))
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
                              .CopyAddressTo(&subheader_title)
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
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                  views::Builder<views::ImageButton>(
                      views::CreateVectorImageButtonWithNativeTheme(
                          base::BindRepeating(
                              &chrome::ShowWebStore, browser_,
                              extension_urls::kExtensionsMenuUtmSource),
                          vector_icons::kGoogleChromeWebstoreIcon, icon_size))
                      .SetAccessibleName(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_MAIN_PAGE_OPEN_CHROME_WEBSTORE_ACCESSIBLE_NAME))
                      .CustomConfigure(
                          base::BindOnce([](views::ImageButton* view) {
                            view->SizeToPreferredSize();
                            InstallCircleHighlightPathGenerator(view);
                          })),
#endif
                  // Setting button.
                  views::Builder<views::ImageButton>(
                      views::CreateVectorImageButtonWithNativeTheme(
                          base::BindRepeating(
                              [](Browser* browser) {
                                base::RecordAction(base::UserMetricsAction(
                                    "Extensions.Menu."
                                    "ExtensionsSettingsOpened"));
                                chrome::ShowExtensions(browser);
                              },
                              browser_),
                          features::IsChromeRefresh2023()
                              ? vector_icons::kSettingsChromeRefreshIcon
                              : vector_icons::kSettingsIcon,
                          icon_size))
                      .SetProperty(
                          views::kMarginsKey,
                          gfx::Insets::TLBR(0, horizontal_spacing, 0, 0))
                      .SetTooltipText(
                          l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS))
                      .CustomConfigure(
                          base::BindOnce([](views::ImageButton* view) {
                            view->SizeToPreferredSize();
                            InstallCircleHighlightPathGenerator(view);
                          })),
                  // Toggle site settings button.
                  views::Builder<views::ToggleButton>()
                      .CopyAddressTo(&site_settings_toggle_)
                      .SetProperty(
                          views::kMarginsKey,
                          gfx::Insets::TLBR(0, horizontal_spacing, 0, 0))
                      .SetAccessibleName(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_SITE_SETTINGS_TOGGLE_ACCESSIBLE_NAME))
                      .SetCallback(base::BindRepeating(
                          [](views::ToggleButton* toggle_button,
                             base::RepeatingCallback<void(bool)> callback) {
                            callback.Run(toggle_button->GetIsOn());
                          },
                          site_settings_toggle_,
                          base::BindRepeating(
                              &ExtensionsMenuHandler::
                                  OnSiteSettingsToggleButtonPressed,
                              base::Unretained(menu_handler))))),
          // Contents.
          views::Builder<views::Separator>().SetProperty(
              views::kMarginsKey, gfx::Insets::VH(vertical_spacing, 0)),
          views::Builder<views::ScrollView>()
              .ClipHeightTo(0, kMaxExtensionButtonsHeightDp)
              .SetDrawOverflowIndicator(false)
              .SetHorizontalScrollBarMode(
                  views::ScrollView::ScrollBarMode::kDisabled)
              .SetContents(
                  views::Builder<views::BoxLayoutView>()
                      .SetOrientation(views::BoxLayout::Orientation::kVertical)
                      // Horizontal dialog margins are added inside the scroll
                      // view contents to have the scroll bar by the dialog
                      // border.
                      .SetInsideBorderInsets(
                          gfx::Insets::VH(0, dialog_insets.left()))
                      .AddChildren(
                          // Message section.
                          views::Builder<MessageSection>(
                              std::make_unique<MessageSection>(
                                  base::BindRepeating(
                                      &ExtensionsMenuHandler::
                                          OnReloadPageButtonClicked,
                                      base::Unretained(menu_handler_)),
                                  base::BindRepeating(
                                      &ExtensionsMenuHandler::
                                          OnAllowExtensionClicked,
                                      base::Unretained(menu_handler_)),
                                  base::BindRepeating(
                                      &ExtensionsMenuHandler::
                                          OnDismissExtensionClicked,
                                      base::Unretained(menu_handler_))))
                              .CopyAddressTo(&message_section_),
                          // Menu items section.
                          views::Builder<views::BoxLayoutView>()
                              .CopyAddressTo(&menu_items_)
                              // Add bottom dialog margins since it's the last
                              // element.
                              .SetProperty(
                                  views::kMarginsKey,
                                  gfx::Insets::TLBR(
                                      0, 0,
                                      dialog_insets.bottom() -
                                          hover_button_vertical_spacing,
                                      0))
                              .SetOrientation(
                                  views::BoxLayout::Orientation::kVertical))))

      .BuildChildren();

  // Align the site setting toggle vertically with the subheader title by
  // getting the label height after construction.
  site_settings_toggle_->SetPreferredSize(
      gfx::Size(site_settings_toggle_->GetPreferredSize().width(),
                subheader_title->GetLineHeight()));
}

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
               site_permissions_button_access);
  menu_items_->AddChildViewAt(std::move(item), index);
}

void ExtensionsMenuMainPageView::RemoveMenuItem(
    const ToolbarActionsModel::ActionId& action_id) {
  views::View* item = GetMenuItem(menu_items_, action_id);
  menu_items_->RemoveChildViewT(item);
}

void ExtensionsMenuMainPageView::UpdateSubheader(
    const std::u16string& current_site,
    bool is_site_settings_toggle_visible,
    bool is_site_settings_toggle_on) {
  subheader_subtitle_->SetText(current_site);

  site_settings_toggle_->SetVisible(is_site_settings_toggle_visible);
  site_settings_toggle_->SetIsOn(is_site_settings_toggle_on);
  site_settings_toggle_->SetTooltipText(
      GetSiteSettingToggleText(is_site_settings_toggle_on));
}

void ExtensionsMenuMainPageView::UpdateMessageSection(
    MessageSectionState state,
    bool has_enterprise_extensions) {
  message_section_->Update(state, has_enterprise_extensions);
}

void ExtensionsMenuMainPageView::AddOrUpdateExtensionRequestingAccess(
    const extensions::ExtensionId& id,
    const std::u16string& name,
    const ui::ImageModel& icon,
    int index) {
  message_section_->AddOrUpdateExtension(id, name, icon, index);
}

void ExtensionsMenuMainPageView::RemoveExtensionRequestingAccess(
    const extensions::ExtensionId& id) {
  message_section_->RemoveExtension(id);
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
ExtensionsMenuMainPageView::GetSubheaderSubtitleTextForTesting() const {
  return subheader_subtitle_->GetText();
}

views::View* ExtensionsMenuMainPageView::GetTextContainerForTesting() {
  return message_section_->GetTextContainerForTesting();  // IN-TEST
}

views::View* ExtensionsMenuMainPageView::GetReloadContainerForTesting() {
  return message_section_->GetReloadContainerForTesting();  // IN-TEST
}

views::View*
ExtensionsMenuMainPageView::GetRequestsAccessContainerForTesting() {
  return message_section_->GetRequestsAccessContainerForTesting();  // IN-TEST
}

std::vector<extensions::ExtensionId>
ExtensionsMenuMainPageView::GetExtensionsRequestingAccessForTesting() {
  return message_section_->GetExtensionsForTesting();  // IN-TEST
}

views::View*
ExtensionsMenuMainPageView::GetExtensionRequestingAccessEntryForTesting(
    const extensions::ExtensionId& extension_id) {
  return message_section_->GetExtensionEntryForTesting(
      extension_id);  // IN-TEST
}

content::WebContents* ExtensionsMenuMainPageView::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

BEGIN_METADATA(ExtensionsMenuMainPageView, views::View)
END_METADATA
