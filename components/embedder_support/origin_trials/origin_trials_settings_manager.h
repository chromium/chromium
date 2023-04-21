// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIALS_SETTINGS_MANAGER_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIALS_SETTINGS_MANAGER_H_

#include "base/values.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trials_settings.mojom.h"

namespace embedder_support {
class OriginTrialsSettingsManager {
 public:
  explicit OriginTrialsSettingsManager();
  ~OriginTrialsSettingsManager();
  // Provide a snapshot of the settings. The returned settings to callers is a
  // copy and callers can manipulate it without affecting the settings stored in
  // the settings manager.
  blink::mojom::OriginTrialsSettingsPtr GetSettings() const;
  void PopulateFromOriginTrialsConfig(
      const base::Value::List& disabled_tokens_list);

 private:
  void SetDisabledTokens(const base::Value::List& disabled_tokens_list);
  blink::mojom::OriginTrialsSettings settings_;
};
}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIALS_SETTINGS_MANAGER_H_
