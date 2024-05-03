// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar_handler.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_actions.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/actions/actions.h"

namespace {
std::optional<side_panel::customize_chrome::mojom::ActionId>
MojoActionForChromeAction(actions::ActionId action_id) {
  switch (action_id) {
    case kActionPrint:
      return side_panel::customize_chrome::mojom::ActionId::kPrint;
    default:
      return std::nullopt;
  }
}

std::optional<actions::ActionId> ChromeActionForMojoAction(
    side_panel::customize_chrome::mojom::ActionId action_id) {
  switch (action_id) {
    case side_panel::customize_chrome::mojom::ActionId::kPrint:
      return kActionPrint;
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

  // TODO(crbug.com/323964782): Include the other pinnable actions - print is
  // the only example for now.

  const actions::ActionItem* const print_action_item =
      actions::ActionManager::Get().FindAction(
          kActionPrint,
          BrowserActions::FromBrowser(browser())->root_action_item());

  // TODO(crbug.com/337938827): GetText() is wrong here; it returns "&Print..."
  // instead of "Print". We my need to introduce new strings instead of reusing
  // the action item text.
  auto print_action = side_panel::customize_chrome::mojom::Action::New(
      MojoActionForChromeAction(kActionPrint).value(),
      base::UTF16ToUTF8(print_action_item->GetText()));
  actions.push_back(std::move(print_action));

  std::move(callback).Run(std::move(actions));
}

void CustomizeToolbarHandler::GetActionPinned(
    side_panel::customize_chrome::mojom::ActionId action_id,
    GetActionPinnedCallback callback) {
  const std::optional<actions::ActionId> chrome_action =
      ChromeActionForMojoAction(action_id);
  if (!chrome_action.has_value()) {
    mojo::ReportBadMessage(
        "GetActionPinned called with an unsupported action.");
    return;
  }
  std::move(callback).Run(PinnedToolbarActionsModel::Get(profile_)->Contains(
      chrome_action.value()));
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
