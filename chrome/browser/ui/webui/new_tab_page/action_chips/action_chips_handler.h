// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

class ActionChipsHandler : public action_chips::mojom::ActionChipsHandler {
 public:
  ActionChipsHandler(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler> receiver,
      Profile* profile,
      content::WebUI* web_ui,
      const TabIdGenerator* tab_id_generator);
  ActionChipsHandler(const ActionChipsHandler&) = delete;
  ActionChipsHandler& operator=(const ActionChipsHandler&) = delete;
  ~ActionChipsHandler() override;

  void GetActionChips(
      base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
          callback) override;

 private:
  mojo::Receiver<action_chips::mojom::ActionChipsHandler> receiver_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebUI> web_ui_;
  raw_ptr<const TabIdGenerator> tab_id_generator_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_HANDLER_H_
