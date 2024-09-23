// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_SEARCH_BUBBLE_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_LENS_SEARCH_BUBBLE_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/lens/core/mojom/search_bubble.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace lens {

class SearchBubblePageHandler : public lens::mojom::SearchBubblePageHandler {
 public:
  SearchBubblePageHandler(
      TopChromeWebUIController* webui_controller,
      mojo::PendingReceiver<lens::mojom::SearchBubblePageHandler> receiver,
      mojo::PendingRemote<lens::mojom::SearchBubblePage> page,
      content::WebContents* web_contents,
      const PrefService* pref_service);
  SearchBubblePageHandler(const SearchBubblePageHandler&) = delete;
  SearchBubblePageHandler& operator=(const SearchBubblePageHandler&) = delete;
  ~SearchBubblePageHandler() override;

  // lens::mojom::SearchBubblePageHandler:
  void ShowUI() override;
  void CloseUI() override;

 private:
  void SetTheme();

  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<const PrefService> pref_service_;

  const raw_ptr<TopChromeWebUIController> webui_controller_;

  mojo::Receiver<lens::mojom::SearchBubblePageHandler> receiver_;
  mojo::Remote<lens::mojom::SearchBubblePage> page_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_SEARCH_BUBBLE_PAGE_HANDLER_H_
