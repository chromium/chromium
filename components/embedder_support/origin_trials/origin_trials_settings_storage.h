// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIALS_SETTINGS_STORAGE_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIALS_SETTINGS_STORAGE_H_

#include "base/values.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trials_settings.mojom.h"

namespace embedder_support {
// This class maintains the state for Origin Trial Settings. This class is
// instantiated by the browser process.
class OriginTrialsSettingsStorage {
 public:
  explicit OriginTrialsSettingsStorage();
  virtual ~OriginTrialsSettingsStorage();

  // Populates the settings. The settings can come from either CLI flags or the
  // PrefService.
  virtual void PopulateSettings(const base::Value::List& disabled_tokens_list);

  // Provide a snapshot of the settings. The returned settings to callers is a
  // copy and callers can manipulate it without affecting the settings stored in
  // the settings storage.
  virtual blink::mojom::OriginTrialsSettingsPtr GetSettings() const;

 private:
  void SetDisabledTokens(const base::Value::List& disabled_tokens_list);
  blink::mojom::OriginTrialsSettings settings_;
};
}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIALS_SETTINGS_STORAGE_H_
