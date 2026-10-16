// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_PLURAL_STRING_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_PLURAL_STRING_PROVIDER_H_

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/resources/cr_components/search/plural_string_provider.mojom.h"

// Serves pluralized strings to a page over Mojo, so it works in any host
// (WebUI, untrusted WebUI, extension pages) without relying on chrome.send.
// Only the message names in the map given at creation are served.
class PluralStringProvider : public plural_string::mojom::PluralStringProvider {
 public:
  // Maps message names to their ICU plural message IDs.
  using MessageIdMap = base::flat_map<std::string, int>;

  // Creates a provider owned by `receiver`.
  static void Create(mojo::PendingReceiver<
                         plural_string::mojom::PluralStringProvider> receiver,
                     MessageIdMap message_ids);

  explicit PluralStringProvider(MessageIdMap message_ids);
  PluralStringProvider(const PluralStringProvider&) = delete;
  PluralStringProvider& operator=(const PluralStringProvider&) = delete;
  ~PluralStringProvider() override;

  // plural_string::mojom::PluralStringProvider:
  void GetPluralString(const std::string& message_name,
                       int32_t count,
                       GetPluralStringCallback callback) override;

 private:
  const MessageIdMap message_ids_;
};

// Returns the plural strings requested by the shared searchbox and composebox
// components (//ui/webui/resources/cr_components/{search,searchbox,composebox})
// via MojoPluralStringProxy.
PluralStringProvider::MessageIdMap GetSearchboxPluralStringIds();

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_PLURAL_STRING_PROVIDER_H_
