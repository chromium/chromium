// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_TOOLBAR_CUSTOMIZE_TOOLBAR_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_TOOLBAR_CUSTOMIZE_TOOLBAR_HANDLER_H_

#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/actions/action_id.h"

class CustomizeToolbarHandler
    : public side_panel::customize_chrome::mojom::CustomizeToolbarHandler,
      PinnedToolbarActionsModel::Observer {
 public:
  CustomizeToolbarHandler(
      mojo::PendingReceiver<
          side_panel::customize_chrome::mojom::CustomizeToolbarHandler> handler,
      mojo::PendingRemote<
          side_panel::customize_chrome::mojom::CustomizeToolbarClient> client,
      Profile* profile,
      content::WebContents* web_contents);

  CustomizeToolbarHandler(const CustomizeToolbarHandler&) = delete;
  CustomizeToolbarHandler& operator=(const CustomizeToolbarHandler&) = delete;

  ~CustomizeToolbarHandler() override;

  // side_panel::customize_chrome::mojom::CustomizeToolbarHandler:
  void ListActions(ListActionsCallback callback) override;
  void GetActionPinned(side_panel::customize_chrome::mojom::ActionId action_id,
                       GetActionPinnedCallback callback) override;
  void PinAction(side_panel::customize_chrome::mojom::ActionId action_id,
                 bool pin) override;

  // PinnedToolbarActionsModel::Observer:
  void OnActionAdded(const actions::ActionId& id) override;
  void OnActionRemoved(const actions::ActionId& id) override;
  void OnActionMoved(const actions::ActionId& id,
                     int from_index,
                     int to_index) override {}
  void OnActionsChanged() override {}

 private:
  void OnActionPinnedChanged(actions::ActionId id, bool pinned);

  const Browser* browser() const;

  mojo::Remote<side_panel::customize_chrome::mojom::CustomizeToolbarClient>
      client_;
  mojo::Receiver<side_panel::customize_chrome::mojom::CustomizeToolbarHandler>
      receiver_;

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_TOOLBAR_CUSTOMIZE_TOOLBAR_HANDLER_H_
