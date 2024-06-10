// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar_handler.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
std::optional<side_panel::customize_chrome::mojom::ActionId>
MojoActionForChromeAction(actions::ActionId action_id) {
  switch (action_id) {
    case kActionSidePanelShowBookmarks:
      return side_panel::customize_chrome::mojom::ActionId::kShowBookmarks;
    case kActionSidePanelShowHistoryCluster:
      return side_panel::customize_chrome::mojom::ActionId::kShowHistoryCluster;
    case kActionSidePanelShowReadAnything:
      return side_panel::customize_chrome::mojom::ActionId::kShowReadAnything;
    case kActionSidePanelShowReadingList:
      return side_panel::customize_chrome::mojom::ActionId::kShowReadingList;
    case kActionSidePanelShowSideSearch:
      return side_panel::customize_chrome::mojom::ActionId::kShowSideSearch;
    case kActionHome:
      return side_panel::customize_chrome::mojom::ActionId::kHome;
    case kActionForward:
      return side_panel::customize_chrome::mojom::ActionId::kForward;
    case kActionNewIncognitoWindow:
      return side_panel::customize_chrome::mojom::ActionId::kNewIncognitoWindow;
    case kActionShowPasswordManager:
      return side_panel::customize_chrome::mojom::ActionId::
          kShowPasswordManager;
    case kActionShowPaymentMethods:
      return side_panel::customize_chrome::mojom::ActionId::kShowPaymentMethods;
    case kActionShowAddresses:
      return side_panel::customize_chrome::mojom::ActionId::kShowAddresses;
    case kActionShowDownloads:
      return side_panel::customize_chrome::mojom::ActionId::kShowDownloads;
    case kActionClearBrowsingData:
      return side_panel::customize_chrome::mojom::ActionId::kClearBrowsingData;
    case kActionPrint:
      return side_panel::customize_chrome::mojom::ActionId::kPrint;
    case kActionShowTranslate:
      return side_panel::customize_chrome::mojom::ActionId::kShowTranslate;
    case kActionSendTabToSelf:
      return side_panel::customize_chrome::mojom::ActionId::kSendTabToSelf;
    case kActionQrCodeGenerator:
      return side_panel::customize_chrome::mojom::ActionId::kQrCodeGenerator;
    case kActionRouteMedia:
      return side_panel::customize_chrome::mojom::ActionId::kRouteMedia;
    case kActionTaskManager:
      return side_panel::customize_chrome::mojom::ActionId::kTaskManager;
    case kActionDevTools:
      return side_panel::customize_chrome::mojom::ActionId::kDevTools;
    case kActionShowChromeLabs:
      return side_panel::customize_chrome::mojom::ActionId::kShowChromeLabs;
    default:
      return std::nullopt;
  }
}

std::optional<actions::ActionId> ChromeActionForMojoAction(
    side_panel::customize_chrome::mojom::ActionId action_id) {
  switch (action_id) {
    case side_panel::customize_chrome::mojom::ActionId::kShowBookmarks:
      return kActionSidePanelShowBookmarks;
    case side_panel::customize_chrome::mojom::ActionId::kShowHistoryCluster:
      return kActionSidePanelShowHistoryCluster;
    case side_panel::customize_chrome::mojom::ActionId::kShowReadAnything:
      return kActionSidePanelShowReadAnything;
    case side_panel::customize_chrome::mojom::ActionId::kShowReadingList:
      return kActionSidePanelShowReadingList;
    case side_panel::customize_chrome::mojom::ActionId::kShowSideSearch:
      return kActionSidePanelShowSideSearch;
    case side_panel::customize_chrome::mojom::ActionId::kHome:
      return kActionHome;
    case side_panel::customize_chrome::mojom::ActionId::kForward:
      return kActionForward;
    case side_panel::customize_chrome::mojom::ActionId::kNewIncognitoWindow:
      return kActionNewIncognitoWindow;
    case side_panel::customize_chrome::mojom::ActionId::kShowPasswordManager:
      return kActionShowPasswordManager;
    case side_panel::customize_chrome::mojom::ActionId::kShowPaymentMethods:
      return kActionShowPaymentMethods;
    case side_panel::customize_chrome::mojom::ActionId::kShowAddresses:
      return kActionShowAddresses;
    case side_panel::customize_chrome::mojom::ActionId::kShowDownloads:
      return kActionShowDownloads;
    case side_panel::customize_chrome::mojom::ActionId::kClearBrowsingData:
      return kActionClearBrowsingData;
    case side_panel::customize_chrome::mojom::ActionId::kPrint:
      return kActionPrint;
    case side_panel::customize_chrome::mojom::ActionId::kShowTranslate:
      return kActionShowTranslate;
    case side_panel::customize_chrome::mojom::ActionId::kSendTabToSelf:
      return kActionSendTabToSelf;
    case side_panel::customize_chrome::mojom::ActionId::kQrCodeGenerator:
      return kActionQrCodeGenerator;
    case side_panel::customize_chrome::mojom::ActionId::kRouteMedia:
      return kActionRouteMedia;
    case side_panel::customize_chrome::mojom::ActionId::kTaskManager:
      return kActionTaskManager;
    case side_panel::customize_chrome::mojom::ActionId::kDevTools:
      return kActionDevTools;
    case side_panel::customize_chrome::mojom::ActionId::kShowChromeLabs:
      return kActionShowChromeLabs;
    default:
      return std::nullopt;
  }
}
}  // namespace

CustomizeToolbarHandler::CustomizeToolbarHandler(
    mojo::PendingReceiver<
        side_panel::customize_chrome::mojom::CustomizeToolbarHandler> handler,
    mojo::PendingRemote<
        side_panel::customize_chrome::mojom::CustomizeToolbarClient> client,
    raw_ptr<Browser> browser)
    : client_(std::move(client)),
      receiver_(this, std::move(handler)),
      browser_(browser),
      model_(PinnedToolbarActionsModel::Get(browser_->profile())) {
  model_observation_.Observe(model_);
}

CustomizeToolbarHandler::~CustomizeToolbarHandler() = default;

void CustomizeToolbarHandler::ListActions(ListActionsCallback callback) {
  std::vector<side_panel::customize_chrome::mojom::ActionPtr> actions;

  actions::ActionItem* const scope_action =
      browser_->browser_actions()->root_action_item();
  const PinnedToolbarActionsModel* const model = model_;

  // TODO(crbug.com/337938827): GetText() is wrong here; it returns "&Print..."
  // instead of "Print". We my need to introduce new strings instead of reusing
  // the action item text.
  const auto add_action =
      [&actions, model, scope_action](
          actions::ActionId id,
          side_panel::customize_chrome::mojom::CategoryId category) {
        const actions::ActionItem* const action_item =
            actions::ActionManager::Get().FindAction(id, scope_action);
        if (!action_item) {
          return;
        }

        auto mojo_action = side_panel::customize_chrome::mojom::Action::New(
            MojoActionForChromeAction(id).value(),
            base::UTF16ToUTF8(action_item->GetText()), model->Contains(id),
            category);
        actions.push_back(std::move(mojo_action));
      };

  // TODO(crbug.com/323961924): Enable the remaining actions as they are created
  // in the action manager.

  // add_action(kActionHome,
  // side_panel::customize_chrome::mojom::CategoryId::kNavigation);
  // add_action(kActionForward,
  // side_panel::customize_chrome::mojom::CategoryId::kNavigation);
  add_action(kActionNewIncognitoWindow,
             side_panel::customize_chrome::mojom::CategoryId::kNavigation);

  // add_action(kActionShowPasswordManager,
  // side_panel::customize_chrome::mojom::CategoryId::kYourChrome);
  // add_action(kActionShowPaymentMethods,
  // side_panel::customize_chrome::mojom::CategoryId::kYourChrome);
  // add_action(kActionShowAddresses,
  // side_panel::customize_chrome::mojom::CategoryId::kYourChrome);
  // add_action(kActionShowDownloads,
  // side_panel::customize_chrome::mojom::CategoryId::kYourChrome);
  add_action(kActionClearBrowsingData,
             side_panel::customize_chrome::mojom::CategoryId::kYourChrome);

  add_action(kActionSidePanelShowBookmarks,
             side_panel::customize_chrome::mojom::CategoryId::kSidePanels);
  add_action(kActionSidePanelShowHistoryCluster,
             side_panel::customize_chrome::mojom::CategoryId::kSidePanels);
  add_action(kActionSidePanelShowReadAnything,
             side_panel::customize_chrome::mojom::CategoryId::kSidePanels);
  add_action(kActionSidePanelShowReadingList,
             side_panel::customize_chrome::mojom::CategoryId::kSidePanels);
  // add_action(kActionSidePanelShowSideSearch,
  // side_panel::customize_chrome::mojom::CategoryId::kSidePanels);

  add_action(kActionPrint,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  // add_action(kActionShowTranslate,
  // side_panel::customize_chrome::mojom::CategoryId::kTools);
  // add_action(kActionSendTabToSelf,
  // side_panel::customize_chrome::mojom::CategoryId::kTools);
  // add_action(kActionQrCodeGenerator,
  // side_panel::customize_chrome::mojom::CategoryId::kTools);
  // add_action(kActionRouteMedia,
  // side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionTaskManager,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionDevTools,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  // add_action(kActionShowChromeLabs,
  // side_panel::customize_chrome::mojom::CategoryId::kTools);

  std::move(callback).Run(std::move(actions));
}

void CustomizeToolbarHandler::ListCategories(ListCategoriesCallback callback) {
  std::vector<side_panel::customize_chrome::mojom::CategoryPtr> categories;

  categories.push_back(side_panel::customize_chrome::mojom::Category::New(
      side_panel::customize_chrome::mojom::CategoryId::kNavigation,
      l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_TOOLBAR_CATEGORY_NAVIGATION)));
  categories.push_back(side_panel::customize_chrome::mojom::Category::New(
      side_panel::customize_chrome::mojom::CategoryId::kYourChrome,
      l10n_util::GetStringUTF8(
          IDS_NTP_CUSTOMIZE_TOOLBAR_CATEGORY_YOUR_CHROME)));
  categories.push_back(side_panel::customize_chrome::mojom::Category::New(
      side_panel::customize_chrome::mojom::CategoryId::kSidePanels,
      l10n_util::GetStringUTF8(
          IDS_NTP_CUSTOMIZE_TOOLBAR_CATEGORY_SIDE_PANELS)));
  categories.push_back(side_panel::customize_chrome::mojom::Category::New(
      side_panel::customize_chrome::mojom::CategoryId::kTools,
      l10n_util::GetStringUTF8(
          IDS_NTP_CUSTOMIZE_TOOLBAR_CATEGORY_TOOLS_AND_ACTIONS)));

  std::move(callback).Run(std::move(categories));
}

void CustomizeToolbarHandler::PinAction(
    side_panel::customize_chrome::mojom::ActionId action_id,
    bool pin) {
  const std::optional<actions::ActionId> chrome_action =
      ChromeActionForMojoAction(action_id);
  if (!chrome_action.has_value()) {
    mojo::ReportBadMessage("PinAction called with an unsupported action.");
    return;
  }
  model_->UpdatePinnedState(chrome_action.value(), pin);
}

void CustomizeToolbarHandler::OnActionAdded(const actions::ActionId& id) {
  OnActionPinnedChanged(id, true);
}

void CustomizeToolbarHandler::OnActionRemoved(const actions::ActionId& id) {
  OnActionPinnedChanged(id, false);
}

void CustomizeToolbarHandler::OnActionPinnedChanged(actions::ActionId id,
                                                    bool pinned) {
  const std::optional<side_panel::customize_chrome::mojom::ActionId>
      mojo_action_id = MojoActionForChromeAction(id);
  if (!mojo_action_id.has_value()) {
    return;
  }

  client_->SetActionPinned(mojo_action_id.value(), pinned);
}
