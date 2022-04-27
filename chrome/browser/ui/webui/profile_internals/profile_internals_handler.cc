// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_internals/profile_internals_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

ProfileInternalsHandler::ProfileInternalsHandler() = default;

ProfileInternalsHandler::~ProfileInternalsHandler() = default;

void ProfileInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getProfilesList",
      base::BindRepeating(&ProfileInternalsHandler::HandleProfilesChanged,
                          base::Unretained(this)));
}

void ProfileInternalsHandler::HandleProfilesChanged(
    const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(0u, args.size());
  base::Value v("Under construction.");
  FireWebUIListener("profiles-changed", v);
}
