// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_LAUNCHED_CONFIGS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_LAUNCHED_CONFIGS_H_

namespace autofill_assistant {

class LaunchedStarterHeuristicConfig;

namespace launched_configs {

// Note: configs are created on first use, mostly to allow unit tests to enable
// or disable base::Features before the instances are created.
LaunchedStarterHeuristicConfig* GetOrCreateShoppingConfig();
LaunchedStarterHeuristicConfig* GetOrCreateCouponsConfig();

}  // namespace launched_configs
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_LAUNCHED_CONFIGS_H_
