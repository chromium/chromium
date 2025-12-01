// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_HANDLER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

class ActionChipsHandler : public action_chips::mojom::ActionChipsHandler,
                           public TabStripModelObserver {
 public:
  ActionChipsHandler(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler> receiver,
      mojo::PendingRemote<action_chips::mojom::Page> page,
      Profile* profile,
      content::WebUI* web_ui,
      std::unique_ptr<ActionChipsGenerator> action_chips_generator);
  ActionChipsHandler(const ActionChipsHandler&) = delete;
  ActionChipsHandler& operator=(const ActionChipsHandler&) = delete;
  ~ActionChipsHandler() override;

  void StartActionChipsRetrieval() override;

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  void SendActionChipsToUi(
      std::vector<action_chips::mojom::ActionChipPtr> chips);

  mojo::Receiver<action_chips::mojom::ActionChipsHandler> receiver_;
  mojo::Remote<action_chips::mojom::Page> page_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebUI> web_ui_;
  std::unique_ptr<ActionChipsGenerator> action_chips_generator_;
  base::WeakPtrFactory<ActionChipsHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_HANDLER_H_
