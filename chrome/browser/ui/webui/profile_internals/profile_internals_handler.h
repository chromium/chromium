// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PROFILE_INTERNALS_PROFILE_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PROFILE_INTERNALS_PROFILE_INTERNALS_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace regional_capabilities {
class CountryIdHolder;
}

// Handles actions on Profile Internals debug page.
class ProfileInternalsHandler : public content::WebUIMessageHandler {
 public:
  ProfileInternalsHandler();

  ProfileInternalsHandler(const ProfileInternalsHandler&) = delete;
  ProfileInternalsHandler& operator=(const ProfileInternalsHandler&) = delete;

  ~ProfileInternalsHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  friend class ProfileInternalsHandlerTest;

  void HandleGetProfilesList(const base::Value::List& args);

  void PushProfilesList();

  static base::Value::Dict CreateProfileEntry(
      const ProfileAttributesEntry* entry);

  static std::string CountryIdToDebugString(
      std::optional<regional_capabilities::CountryIdHolder> country_id);

  // Returns the list of profiles ordered by the local profile name.
  base::Value::List GetProfilesList();
};

#endif  // CHROME_BROWSER_UI_WEBUI_PROFILE_INTERNALS_PROFILE_INTERNALS_HANDLER_H_
