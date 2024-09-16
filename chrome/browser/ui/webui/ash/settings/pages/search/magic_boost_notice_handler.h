// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SEARCH_MAGIC_BOOST_NOTICE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SEARCH_MAGIC_BOOST_NOTICE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/settings/pages/search/mojom/magic_boost_handler.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace ash::settings {

// Magic boost Notice handler.
class MagicBoostNoticeHandler : public magic_boost_handler::mojom::PageHandler {
 public:
  MagicBoostNoticeHandler(
      mojo::PendingReceiver<magic_boost_handler::mojom::PageHandler> receiver,
      Profile* profile);

  MagicBoostNoticeHandler(const MagicBoostNoticeHandler&) = delete;
  MagicBoostNoticeHandler& operator=(const MagicBoostNoticeHandler&) = delete;

  ~MagicBoostNoticeHandler() override;

 private:
  // magic_boost_handler::mojom::PageHandler:
  void ShowNotice() override;

  mojo::Receiver<magic_boost_handler::mojom::PageHandler> receiver_{this};

  const raw_ptr<Profile> profile_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SEARCH_MAGIC_BOOST_NOTICE_HANDLER_H_
