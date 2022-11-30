// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_FINCH_CONFIGS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_FINCH_CONFIGS_H_

namespace autofill_assistant {

class FinchStarterHeuristicConfig;

namespace finch_configs {

// Starter heuristic instances from finch, to be shared between tabs.
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic1();
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic2();
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic3();
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic4();
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic5();
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic6();
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic7();
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic8();
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic9();

}  // namespace finch_configs
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_FINCH_CONFIGS_H_
