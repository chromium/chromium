// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar_handler.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/actions/actions.h"

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
    Profile* profile,
    content::WebContents* web_contents)
    : client_(std::move(client)),
      receiver_(this, std::move(handler)),
      profile_(profile),
      web_contents_(web_contents) {
  PinnedToolbarActionsModel::Get(profile_)->AddObserver(this);
}

CustomizeToolbarHandler::~CustomizeToolbarHandler() {
  PinnedToolbarActionsModel::Get(profile_)->RemoveObserver(this);
}

void CustomizeToolbarHandler::ListActions(ListActionsCallback callback) {
  std::vector<side_panel::customize_chrome::mojom::ActionPtr> actions;

  actions::ActionItem* const scope_action =
      browser()->browser_actions()->root_action_item();
  const PinnedToolbarActionsModel* const pinned_model =
      PinnedToolbarActionsModel::Get(profile_);

  // TODO(crbug.com/337938827): GetText() is wrong here; it returns "&Print..."
  // instead of "Print". We my need to introduce new strings instead of reusing
  // the action item text.
  const auto add_action = [&actions, pinned_model,
                           scope_action](actions::ActionId id) {
    const actions::ActionItem* const action_item =
        actions::ActionManager::Get().FindAction(id, scope_action);
    CHECK(action_item) << "Action with id " << id << " not found.";
    auto mojo_action = side_panel::customize_chrome::mojom::Action::New(
        MojoActionForChromeAction(id).value(),
        base::UTF16ToUTF8(action_item->GetText()), pinned_model->Contains(id));
    actions.push_back(std::move(mojo_action));
  };

  // TODO(crbug.com/323961924): Enable the remaining actions as they are created
  // in the action manager.
  add_action(kActionSidePanelShowBookmarks);
  add_action(kActionSidePanelShowHistoryCluster);
  add_action(kActionSidePanelShowReadAnything);
  add_action(kActionSidePanelShowReadingList);
  // add_action(kActionSidePanelShowSideSearch);

  // add_action(kActionHome);
  // add_action(kActionForward);
  add_action(kActionNewIncognitoWindow);
  // add_action(kActionShowPasswordManager);
  // add_action(kActionShowPaymentMethods);
  // add_action(kActionShowAddresses);
  // add_action(kActionShowDownloads);
  add_action(kActionClearBrowsingData);
  add_action(kActionPrint);
  // add_action(kActionShowTranslate);
  // add_action(kActionSendTabToSelf);
  // add_action(kActionQrCodeGenerator);
  // add_action(kActionRouteMedia);
  add_action(kActionTaskManager);
  add_action(kActionDevTools);
  // add_action(kActionShowChromeLabs);

  std::move(callback).Run(std::move(actions));
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
  PinnedToolbarActionsModel::Get(profile_)->UpdatePinnedState(
      chrome_action.value(), pin);
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

const Browser* CustomizeToolbarHandler::browser() const {
  const Browser* const browser =
      chrome::FindBrowserWithWindow(web_contents_->GetTopLevelNativeWindow());
  CHECK(browser);
  return browser;
}
