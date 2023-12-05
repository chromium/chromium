// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_HANDLER_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals.mojom.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

class PrivacySandboxInternalsHandler
    : public privacy_sandbox_internals::mojom::PageHandler {
 public:
  explicit PrivacySandboxInternalsHandler(Profile* profile);

  ~PrivacySandboxInternalsHandler() override;

  // Movable, not copyable.
  PrivacySandboxInternalsHandler(PrivacySandboxInternalsHandler&&) = default;
  PrivacySandboxInternalsHandler(const PrivacySandboxInternalsHandler&) =
      delete;
  PrivacySandboxInternalsHandler& operator=(
      const PrivacySandboxInternalsHandler&) = delete;

  void GetCookieContentSettings(
      GetCookieContentSettingsCallback callback) override;
  void ContentSettingsPatternToString(
      const ContentSettingsPattern& pattern,
      ContentSettingsPatternToStringCallback callback) override;

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_INTERNALS_HANDLER_H_
