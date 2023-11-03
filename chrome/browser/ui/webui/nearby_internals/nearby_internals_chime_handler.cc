// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_chime_handler.h"

NearbyInternalsChimeHandler::NearbyInternalsChimeHandler() = default;

NearbyInternalsChimeHandler::~NearbyInternalsChimeHandler() = default;

void NearbyInternalsChimeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "AddChimeClient",
      base::BindRepeating(&NearbyInternalsChimeHandler::HandleAddChimeClient,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "InitializeChimeHandler",
      base::BindRepeating(&NearbyInternalsChimeHandler::Initialize,
                          base::Unretained(this)));
}

void NearbyInternalsChimeHandler::OnJavascriptAllowed() {}

void NearbyInternalsChimeHandler::OnJavascriptDisallowed() {}

void NearbyInternalsChimeHandler::Initialize(const base::Value::List& args) {
  AllowJavascript();
}

// TODO(b/306399642): Once the Chime `KeyedService` and `ChimeClient` base class
// is created, this function will be used to retrieve the service and then add
// `NearbyInternalsChimeHandler` as a `ChimeClient` to `ChimeClientManager`.
void NearbyInternalsChimeHandler::HandleAddChimeClient(
    const base::Value::List& args) {}
