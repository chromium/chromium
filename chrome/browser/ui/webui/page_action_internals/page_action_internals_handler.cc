// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/page_action_internals/page_action_internals_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/menus/simple_menu_model.h"

namespace {

constexpr actions::ActionId kActionId = kActionFakePageActionForDebug;

class FakeMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  bool IsCommandIdChecked(int command_id) const override { return false; }
  bool IsCommandIdEnabled(int command_id) const override { return true; }
  void ExecuteCommand(int command_id, int event_flags) override {}
};

FakeMenuDelegate* GetFakeMenuDelegate() {
  static base::NoDestructor<FakeMenuDelegate> delegate;
  return delegate.get();
}

// Creates fake favicons for the internals page to help visualize icon stacking
// in the UI.
class FaviconSource : public gfx::CanvasImageSource {
 public:
  FaviconSource(SkColor color, char16_t letter)
      : gfx::CanvasImageSource(gfx::Size(16, 16)),
        color_(color),
        letter_(letter) {}

  FaviconSource(const FaviconSource&) = delete;
  FaviconSource& operator=(const FaviconSource&) = delete;
  ~FaviconSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setColor(color_);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    canvas->DrawRoundRect(gfx::Rect(0, 0, 16, 16), 4, flags);

    std::u16string text(1, letter_);
    gfx::FontList font_list;
    canvas->DrawStringRectWithFlags(text, font_list, SK_ColorWHITE,
                                    gfx::Rect(0, 0, 16, 16),
                                    gfx::Canvas::TEXT_ALIGN_CENTER);
  }

 private:
  SkColor color_;
  char16_t letter_;
};

ui::ImageModel CreateFakeFavicon(SkColor color, char16_t letter) {
  return ui::ImageModel::FromImageSkia(
      gfx::CanvasImageSource::MakeImageSkia<FaviconSource>(color, letter));
}

std::optional<ui::ImageModel> MapIcon(
    std::optional<page_action_internals::mojom::IconType> type) {
  if (!type) {
    return std::nullopt;
  }
  switch (*type) {
    case page_action_internals::mojom::IconType::kInfo:
      return ui::ImageModel::FromVectorIcon(vector_icons::kInfoIcon,
                                            ui::kColorSysPrimary, 16);
    case page_action_internals::mojom::IconType::kOrangeAFavicon:
      return CreateFakeFavicon(SkColorSetRGB(255, 153, 0), u'A');
  }
}

page_actions::AnchoredMessageActionIconType MapActionButton(
    std::optional<page_action_internals::mojom::ActionButtonType> type) {
  if (!type) {
    return page_actions::AnchoredMessageActionIconType::kNone;
  }
  switch (*type) {
    case page_action_internals::mojom::ActionButtonType::kClose:
      return page_actions::AnchoredMessageActionIconType::kClose;
    case page_action_internals::mojom::ActionButtonType::kMenu:
      return page_actions::AnchoredMessageActionIconType::kMenu;
  }
}

void InitializeActionItemDefaults() {
  BrowserWindowInterface* bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (!bwi) {
    return;
  }
  auto* browser_actions = BrowserActions::From(bwi);
  if (!browser_actions) {
    return;
  }
  auto* root_action_item = browser_actions->root_action_item();
  if (!root_action_item) {
    return;
  }
  actions::ActionItem* item =
      actions::ActionManager::Get().FindAction(kActionId, root_action_item);
  if (!item) {
    return;
  }
  item->SetText(u"Fake Page Action");
  item->SetTooltipText(u"Fake Page Action for debugging");
  item->SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kInfoIcon,
                                                ui::kColorSysPrimary, 16));
}

}  // namespace

PageActionInternalsHandler::PageActionInternalsHandler(
    mojo::PendingReceiver<page_action_internals::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

PageActionInternalsHandler::~PageActionInternalsHandler() = default;

page_actions::PageActionController*
PageActionInternalsHandler::GetActiveController() {
  BrowserWindowInterface* const bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (!bwi) {
    return nullptr;
  }

  tabs::TabInterface* const tab = bwi->GetActiveTabInterface();
  if (!tab) {
    return nullptr;
  }

  return page_actions::PageActionController::From(tab);
}

void PageActionInternalsHandler::ShowPageAction(
    page_action_internals::mojom::PageActionParamsPtr params,
    ShowPageActionCallback callback) {
  page_actions::PageActionController* controller = GetActiveController();
  if (!controller) {
    std::move(callback).Run(false);
    return;
  }

  InitializeActionItemDefaults();

  controller->Show(kActionId);

  auto icon = MapIcon(params->icon_type);
  if (icon) {
    controller->OverrideImage(kActionId, *icon);
  } else {
    controller->ClearOverrideImage(kActionId);
  }

  if (params->chip_text) {
    controller->OverrideText(kActionId, *params->chip_text);
    page_actions::SuggestionChipConfig chip_config;
    chip_config.priority =
        page_actions::PageActionPriorityCategory::kCoreSiteUtility;
    chip_config.should_animate = true;
    controller->ShowSuggestionChip(kActionId, chip_config);
  } else {
    controller->HideSuggestionChip(kActionId);
    controller->ClearOverrideText(kActionId);
  }

  std::move(callback).Run(true);
}

void PageActionInternalsHandler::ShowAnchoredMessage(
    page_action_internals::mojom::AnchoredMessageParamsPtr params,
    ShowAnchoredMessageCallback callback) {
  page_actions::PageActionController* controller = GetActiveController();
  if (!controller) {
    std::move(callback).Run(false);
    return;
  }

  InitializeActionItemDefaults();

  controller->SetAnchoredMessageText(kActionId, params->message_text);

  auto icon = MapIcon(params->bubble_icon);
  if (icon) {
    controller->SetAnchoredMessageIcon(kActionId, *icon);
  } else {
    controller->ClearAnchoredMessageIcon(kActionId);
  }

  auto action_icon_type = MapActionButton(params->action_icon);
  std::unique_ptr<ui::SimpleMenuModel> menu_model;
  if (action_icon_type == page_actions::AnchoredMessageActionIconType::kMenu) {
    menu_model = std::make_unique<ui::SimpleMenuModel>(GetFakeMenuDelegate());
    menu_model->AddItem(1, u"Menu Item 1");
  }
  controller->SetAnchoredMessageAction(kActionId, action_icon_type,
                                       std::move(menu_model));

  if (params->expandable_content) {
    page_actions::AnchoredMessageExpandableContent content;
    content.heading = params->expandable_content->heading;
    for (const auto& item : params->expandable_content->items) {
      page_actions::AnchoredMessageExpandableItem view_item;
      view_item.text = item->text;
      view_item.icon = MapIcon(item->icon_type);
      content.items.push_back(std::move(view_item));
    }
    controller->SetAnchoredMessageExpandableContent(kActionId,
                                                    std::move(content));
  } else {
    controller->SetAnchoredMessageExpandableContent(kActionId, std::nullopt);
  }

  page_actions::AnchoredMessageConfig config;
  config.priority = page_actions::PageActionPriorityCategory::kUserInteraction;

  // Make sure the page action itself is "shown" so the anchor exists.
  controller->Show(kActionId);
  controller->ShowAnchoredMessage(kActionId, config);

  std::move(callback).Run(true);
}

void PageActionInternalsHandler::HidePageAction(
    HidePageActionCallback callback) {
  page_actions::PageActionController* controller = GetActiveController();
  if (!controller) {
    std::move(callback).Run(false);
    return;
  }

  controller->HideAnchoredMessage(kActionId);
  controller->HideSuggestionChip(kActionId);
  controller->Hide(kActionId);
  std::move(callback).Run(true);
}
