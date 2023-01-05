// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list.mojom.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

class ReadingListPageHandler;

class ReadingListUI : public ui::MojoBubbleWebUIController,
                      public reading_list::mojom::PageHandlerFactory,
                      public help_bubble::mojom::HelpBubbleHandlerFactory {
 public:
  explicit ReadingListUI(content::WebUI* web_ui);
  ReadingListUI(const ReadingListUI&) = delete;
  ReadingListUI& operator=(const ReadingListUI&) = delete;
  ~ReadingListUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<reading_list::mojom::PageHandlerFactory> receiver);

  void BindInterface(
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
          pending_receiver);

  void SetActiveTabURL(const GURL& url);

 private:
  // reading_list::mojom::PageHandlerFactory:
  void CreatePageHandler(mojo::PendingRemote<reading_list::mojom::Page> page,
                         mojo::PendingReceiver<reading_list::mojom::PageHandler>
                             receiver) override;

  // help_bubble::mojom::HelpBubbleHandlerFactory:
  void CreateHelpBubbleHandler(
      mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
      mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler)
      override;

  std::unique_ptr<ReadingListPageHandler> page_handler_;
  mojo::Receiver<reading_list::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  std::unique_ptr<user_education::HelpBubbleHandler> help_bubble_handler_;
  mojo::Receiver<help_bubble::mojom::HelpBubbleHandlerFactory>
      help_bubble_handler_factory_receiver_{this};

  WebuiLoadTimer webui_load_timer_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READING_LIST_READING_LIST_UI_H_
