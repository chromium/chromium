// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SEARCH_MAGIC_BOOST_NOTICE_PAGE_HANDLER_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SEARCH_MAGIC_BOOST_NOTICE_PAGE_HANDLER_FACTORY_H_

#include <memory>

#include "chrome/browser/ui/webui/ash/settings/pages/search/magic_boost_notice_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/search/mojom/magic_boost_handler.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace ash::settings {

class MagicBoostNoticePageHandlerFactory
    : public magic_boost_handler::mojom::PageHandlerFactory {
 public:
  MagicBoostNoticePageHandlerFactory(
      Profile* profile,
      mojo::PendingReceiver<magic_boost_handler::mojom::PageHandlerFactory>
          receiver);

  MagicBoostNoticePageHandlerFactory(
      const MagicBoostNoticePageHandlerFactory&) = delete;
  MagicBoostNoticePageHandlerFactory& operator=(
      const MagicBoostNoticePageHandlerFactory&) = delete;

  ~MagicBoostNoticePageHandlerFactory() override;

 private:
  // magic_boost_handler::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<magic_boost_handler::mojom::PageHandler> receiver)
      override;

  raw_ptr<Profile> profile_;

  std::unique_ptr<MagicBoostNoticeHandler> page_handler_;
  mojo::Receiver<magic_boost_handler::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_SEARCH_MAGIC_BOOST_NOTICE_PAGE_HANDLER_FACTORY_H_
