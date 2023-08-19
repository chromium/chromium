// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_handler.h"

#include "components/signin/public/base/signin_switches.h"

SearchEngineChoiceHandler::SearchEngineChoiceHandler(
    mojo::PendingReceiver<search_engine_choice::mojom::PageHandler> receiver,
    base::OnceCallback<void(int)> display_dialog_callback)
    : receiver_(this, std::move(receiver)),
      display_dialog_callback_(std::move(display_dialog_callback)) {
  CHECK(base::FeatureList::IsEnabled(switches::kSearchEngineChoice));
  // `display_dialog_callback` being null would indicate that the handler is
  // created before calling `SearchEngineChoiceUI::Initialize()`, which should
  // never happen.
  CHECK(display_dialog_callback_);
}

SearchEngineChoiceHandler::~SearchEngineChoiceHandler() = default;

void SearchEngineChoiceHandler::DisplayDialog(uint32_t content_height) {
  if (display_dialog_callback_) {
    std::move(display_dialog_callback_).Run(content_height);
  }
}
