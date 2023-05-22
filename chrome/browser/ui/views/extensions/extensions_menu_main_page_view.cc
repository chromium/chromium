// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_main_page_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_handler.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_frame_view.h"
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

// View that contains a special message inside the extensions menu main page
// depending on its state.
class MessageSection : public views::BoxLayoutView {
 public:
  explicit MessageSection(
      base::RepeatingCallback<void(const extensions::ExtensionId&)>
          allow_callback);
  MessageSection(const MessageSection&) = delete;
  const MessageSection& operator=(const MessageSection&) = delete;
  ~MessageSection() override = default;

  // Updates the views contents and visibility given `state`. At most only one
  // of the "containers" will be visible per `state`.
  void Update(ExtensionsMenuMainPageView::MessageSectionState state);

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
  views::Label* GetTextContainerForTesting() { return text_container_; }
  views::View* GetRequestsAccessContainerForTesting() {
    return requests_access_container_;
  }
  std::vector<extensions::ExtensionId> GetExtensionsForTesting();
  views::View* GetExtensionEntryForTesting(
      const extensions::ExtensionId& extension_id);

 private:
  static constexpr int kExtensionItemsContainerIndex = 1;
  static constexpr int kExtensionItemIconIndex = 0;
  static constexpr int kExtensionItemLabelIndex = 1;

  // Removes all extension entries.
  void ClearExtensions();

  // The current state of the section.
  ExtensionsMenuMainPageView::MessageSectionState state_;

  // Text container.
  raw_ptr<views::Label> text_container_;

  // Request access container
  raw_ptr<views::View> requests_access_container_;
  // A collection of all the extension entries in the request access container.
  std::map<extensions::ExtensionId, views::View*> extension_entries_;

  // Callback for the allow button for the extension entries.
  base::RepeatingCallback<void(const extensions::ExtensionId&)> allow_callback_;
};

BEGIN_VIEW_BUILDER(/* No Export */, MessageSection, views::BoxLayoutView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* No Export */, MessageSection)

MessageSection::MessageSection(
    base::RepeatingCallback<void(const extensions::ExtensionId&)>
        allow_callback)
    : allow_callback_(std::move(allow_callback)) {
  views::Builder<MessageSection>(this)
      .SetOrientation(views::BoxLayout::Orientation::kVertical)
      // TODO(crbug.com/1390952): After adding margins, compute radius from a
      // variable or create a const variable.
      .SetBackground(views::CreateThemedRoundedRectBackground(
          kColorExtensionsMenuHighlightedBackground, 4))
      .AddChildren(
          // Text container.
          views::Builder<views::Label>()
              .CopyAddressTo(&text_container_)
              .SetVisible(false)
              .SetTextContext(ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
              .SetHorizontalAlignment(gfx::ALIGN_CENTER),
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
    ExtensionsMenuMainPageView::MessageSectionState state) {
  state_ = state;
  switch (state_) {
    case ExtensionsMenuMainPageView::MessageSectionState::kRestrictedAccess:
      text_container_->SetText(l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_RESTRICTED_ACCESS_TEXT));
      text_container_->SetVisible(true);
      requests_access_container_->SetVisible(false);
      ClearExtensions();
      break;
    case ExtensionsMenuMainPageView::MessageSectionState::kUserCustomizedAccess:
      text_container_->SetVisible(false);
      requests_access_container_->SetVisible(!extension_entries_.empty());
      break;
    case ExtensionsMenuMainPageView::MessageSectionState::kUserBlockedAcces:
      text_container_->SetText(l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_MENU_MESSAGE_SECTION_USER_BLOCKED_ACCESS_TEXT));
      text_container_->SetVisible(true);
      requests_access_container_->SetVisible(false);
      ClearExtensions();
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
    auto item =
        views::Builder<views::FlexLayoutView>()
            .SetOrientation(views::LayoutOrientation::kHorizontal)
            .AddChildren(
                views::Builder<views::ImageView>().SetImage(icon),
                views::Builder<views::Label>().SetText(name),
                views::Builder<views::MdTextButton>()
                    .SetCallback(base::BindRepeating(allow_callback_, id))
                    .SetText(l10n_util::GetStringUTF16(
                        IDS_EXTENSIONS_MENU_REQUESTS_ACCESS_SECTION_ALLOW_BUTTON_TEXT)))
            .Build();
    extension_entries_.insert({id, item.get()});
    requests_access_container_->children()[1]->AddChildViewAt(std::move(item),
                                                              index);

    requests_access_container_->SetVisible(!extension_entries_.empty());
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
}

void MessageSection::ClearExtensions() {
  requests_access_container_->children()[kExtensionItemsContainerIndex]
      ->RemoveAllChildViews();
  extension_entries_.clear();
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
// TODO(crbug.com/1390952): Move webstore, setting, and toggle
// button under close button. This will be done as part of
// adding margins to the menu.
// Webstore button.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
                  views::Builder<views::ImageButton>(
                      views::CreateVectorImageButtonWithNativeTheme(
                          base::BindRepeating(
                              &chrome::ShowWebStore, browser_,
                              extension_urls::kExtensionsMenuUtmSource),
                          vector_icons::kGoogleChromeWebstoreIcon))
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
                              &ExtensionsMenuHandler::CloseBubble,
                              base::Unretained(menu_handler_))))),
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
                          // Message section.
                          views::Builder<MessageSection>(
                              std::make_unique<MessageSection>(
                                  base::BindRepeating(
                                      &ExtensionsMenuHandler::
                                          OnAllowExtensionClicked,
                                      base::Unretained(menu_handler_))))
                              .CopyAddressTo(&message_section_),
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

void ExtensionsMenuMainPageView::OnToggleButtonPressed() {
  const url::Origin& origin =
      GetActiveWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  PermissionsManager::UserSiteSetting site_setting =
      site_settings_toggle_->GetIsOn()
          ? PermissionsManager::UserSiteSetting::kCustomizeByExtension
          : PermissionsManager::UserSiteSetting::kBlockAllExtensions;

  PermissionsManager::Get(browser_->profile())
      ->UpdateUserSiteSetting(origin, site_setting);

  // TODO(crbug.com/1390952): Show reload message in menu if any extension needs
  // a page refresh for the update to take effect.
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
  site_settings_toggle_->SetAccessibleName(
      GetSiteSettingToggleText(is_site_settings_toggle_on));
}

void ExtensionsMenuMainPageView::UpdateMessageSection(
    MessageSectionState state) {
  message_section_->Update(state);
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
}

std::vector<ExtensionMenuItemView*> ExtensionsMenuMainPageView::GetMenuItems()
    const {
  std::vector<ExtensionMenuItemView*> menu_item_views;
  for (views::View* view : menu_items_->children()) {
    menu_item_views.push_back(GetAsMenuItem(view));
  }
  return menu_item_views;
}

views::Label* ExtensionsMenuMainPageView::GetTextContainerForTesting() {
  return message_section_->GetTextContainerForTesting();
}
views::View*
ExtensionsMenuMainPageView::GetRequestsAccessContainerForTesting() {
  return message_section_->GetRequestsAccessContainerForTesting();
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
