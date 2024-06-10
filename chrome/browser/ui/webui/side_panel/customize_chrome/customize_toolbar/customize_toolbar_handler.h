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
      public PinnedToolbarActionsModel::Observer {
 public:
  CustomizeToolbarHandler(
      mojo::PendingReceiver<
          side_panel::customize_chrome::mojom::CustomizeToolbarHandler> handler,
      mojo::PendingRemote<
          side_panel::customize_chrome::mojom::CustomizeToolbarClient> client,
      raw_ptr<Browser> browser);

  CustomizeToolbarHandler(const CustomizeToolbarHandler&) = delete;
  CustomizeToolbarHandler& operator=(const CustomizeToolbarHandler&) = delete;

  ~CustomizeToolbarHandler() override;

  // side_panel::customize_chrome::mojom::CustomizeToolbarHandler:
  void ListActions(ListActionsCallback callback) override;
  void ListCategories(ListCategoriesCallback callback) override;
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

  mojo::Remote<side_panel::customize_chrome::mojom::CustomizeToolbarClient>
      client_;
  mojo::Receiver<side_panel::customize_chrome::mojom::CustomizeToolbarHandler>
      receiver_;

  raw_ptr<Browser> browser_;
  raw_ptr<PinnedToolbarActionsModel> model_;

  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_TOOLBAR_CUSTOMIZE_TOOLBAR_HANDLER_H_
