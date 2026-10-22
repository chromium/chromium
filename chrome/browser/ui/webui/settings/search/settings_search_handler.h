// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"
#include "chrome/browser/ui/webui/settings/search/mojom/settings_search.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace settings_search {

// Implements mojom::SettingsSearchHandler by running FuzzyFinder over a fixed
// list of SettingsSearchItems.
//
// TODO(b/571663356): Take the items from the SettingsSearchRegistry instead of
// a list supplied at construction.
class SettingsSearchHandler : public mojom::SettingsSearchHandler {
 public:
  // TODO(b/574638277): Subject to change based on metrics/product decisions.
  static constexpr size_t kMaxQueryLength = 30;
  static constexpr uint32_t kMaxResults = 6;

  SettingsSearchHandler(
      mojo::PendingReceiver<mojom::SettingsSearchHandler> receiver,
      std::vector<std::unique_ptr<SettingsSearchItem>> items);

  SettingsSearchHandler(const SettingsSearchHandler&) = delete;
  SettingsSearchHandler& operator=(const SettingsSearchHandler&) = delete;
  ~SettingsSearchHandler() override;

  // mojom::SettingsSearchHandler:
  void Search(const std::u16string& query,
              uint32_t max_results,
              SearchCallback callback) override;

 private:
  // The items being searched. `finder_` keeps raw pointers to these, so
  // `finder_` must be destroyed first.
  std::vector<std::unique_ptr<SettingsSearchItem>> items_;
  FuzzyFinder finder_;

  mojo::Receiver<mojom::SettingsSearchHandler> receiver_;
};

}  // namespace settings_search

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_HANDLER_H_
