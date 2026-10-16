// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/plural_string_provider.h"

#include <memory>
#include <utility>

#include "chrome/grit/generated_resources.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/base/l10n/l10n_util.h"

// static
void PluralStringProvider::Create(
    mojo::PendingReceiver<plural_string::mojom::PluralStringProvider> receiver,
    MessageIdMap message_ids) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<PluralStringProvider>(std::move(message_ids)),
      std::move(receiver));
}

PluralStringProvider::PluralStringProvider(MessageIdMap message_ids)
    : message_ids_(std::move(message_ids)) {}

PluralStringProvider::~PluralStringProvider() = default;

void PluralStringProvider::GetPluralString(const std::string& message_name,
                                           int32_t count,
                                           GetPluralStringCallback callback) {
  auto it = message_ids_.find(message_name);
  if (it == message_ids_.end()) {
    mojo::ReportBadMessage("Unknown plural string message name");
    std::move(callback).Run(std::string());
    return;
  }
  if (count < 0) {
    mojo::ReportBadMessage("Negative plural string count");
    std::move(callback).Run(std::string());
    return;
  }
  std::move(callback).Run(l10n_util::GetPluralStringFUTF8(it->second, count));
}

PluralStringProvider::MessageIdMap GetSearchboxPluralStringIds() {
  // Names must match those passed to MojoPluralStringProxy.getPluralString()
  // by the shared components.
  return {
      {"sharingTabs", IDS_COMPOSE_SHARING_TABS},
  };
}
