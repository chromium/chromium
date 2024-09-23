// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_HANDLER_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals.mojom.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace privacy_sandbox_internals {

// Mojo handler for the Privacy Sandbox DevUI page
class PrivacySandboxInternalsHandler
    : public privacy_sandbox_internals::mojom::PageHandler {
 public:
  explicit PrivacySandboxInternalsHandler(
      Profile* profile,
      mojo::PendingReceiver<privacy_sandbox_internals::mojom::PageHandler>
          pending_receiver);

  ~PrivacySandboxInternalsHandler() override;

  PrivacySandboxInternalsHandler(PrivacySandboxInternalsHandler&&) = delete;
  PrivacySandboxInternalsHandler(const PrivacySandboxInternalsHandler&) =
      delete;
  PrivacySandboxInternalsHandler& operator=(
      const PrivacySandboxInternalsHandler&) = delete;

  void ReadPref(const std::string& pref_name,
                ReadPrefCallback callback) override;
  void ReadContentSettings(const ContentSettingsType type,
                           ReadContentSettingsCallback callback) override;

  void ContentSettingsPatternToString(
      const ContentSettingsPattern& pattern,
      ContentSettingsPatternToStringCallback callback) override;

  void StringToContentSettingsPattern(
      const std::string& s,
      StringToContentSettingsPatternCallback callback) override;

  void GetTpcdMetadataGrants(GetTpcdMetadataGrantsCallback callback) override;

 private:
  raw_ptr<Profile, DanglingUntriaged> profile_;
  // It seems like the handler is supposed to retain ownership even though we
  // don't need to reference the mojo::Receiver.
  mojo::Receiver<privacy_sandbox_internals::mojom::PageHandler> receiver_{this};
};

}  // namespace privacy_sandbox_internals

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_HANDLER_H_
