// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_HANDLER_H_

#include <cstdint>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/settings/search/mojom/settings_search.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace settings_search {

class SettingsSearchRegistry;

// Implements mojom::SettingsSearchHandler by running FuzzyFinder over the items
// in a SettingsSearchRegistry.
class SettingsSearchHandler : public mojom::SettingsSearchHandler {
 public:
  // TODO(b/574638277): Subject to change based on metrics/product decisions.
  static constexpr size_t kMaxQueryLength = 30;
  static constexpr uint32_t kMaxResults = 6;

  // `registry` must outlive `this`.
  SettingsSearchHandler(
      mojo::PendingReceiver<mojom::SettingsSearchHandler> receiver,
      SettingsSearchRegistry* registry);

  SettingsSearchHandler(const SettingsSearchHandler&) = delete;
  SettingsSearchHandler& operator=(const SettingsSearchHandler&) = delete;
  ~SettingsSearchHandler() override;

  // mojom::SettingsSearchHandler:
  void Search(const std::u16string& query,
              uint32_t max_results,
              SearchCallback callback) override;

 private:
  const raw_ptr<SettingsSearchRegistry> registry_;
  mojo::Receiver<mojom::SettingsSearchHandler> receiver_;
};

}  // namespace settings_search

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_HANDLER_H_
