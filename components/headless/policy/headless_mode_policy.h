// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_POLICY_HEADLESS_MODE_POLICY_H_
#define COMPONENTS_HEADLESS_POLICY_HEADLESS_MODE_POLICY_H_

class PrefService;

namespace headless {

// Headless mode policy helpers.
class HeadlessModePolicy {
 public:
  // Headless mode as set by policy. The values must match the HeadlessMode
  // policy template in components/policy/resources/policy_templates.json
  enum class HeadlessMode {
    kEnabled = 1,
    kDisabled = 2,
    // Default value to ensure consistency.
    kDefaultValue = kEnabled,
    // Min and max values for range checking.
    kMinValue = kEnabled,
    kMaxValue = kDisabled
  };

  // Returns the current HeadlessMode policy according to the value in
  // |pref_service|. If HeadlessMode policy is not set, the default value
  // |HeadlessMode::kEnabled| will be returned.
  static HeadlessMode GetPolicy(const PrefService* pref_service);

  // Returns positive if current HeadlessMode policy in |pref_service| is set to
  // |HeadlessMode::kDisabled|
  static bool IsHeadlessModeDisabled(const PrefService* pref_service);
};

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_POLICY_HEADLESS_MODE_POLICY_H_
