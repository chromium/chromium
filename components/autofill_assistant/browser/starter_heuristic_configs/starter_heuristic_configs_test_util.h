// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_STARTER_HEURISTIC_CONFIGS_TEST_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_STARTER_HEURISTIC_CONFIGS_TEST_UTIL_H_

#include "components/autofill_assistant/browser/fake_common_dependencies.h"
#include "components/autofill_assistant/browser/fake_starter_platform_delegate.h"

namespace autofill_assistant {
namespace starter_heuristic_configs_test_util {

// Used to parametrize tests.
struct ClientState {
  bool is_custom_tab = false;
  bool is_weblayer = false;
  bool is_logged_in = false;
  bool msbb_enabled = false;
  bool is_supervised_user = false;
  bool proactive_help_enabled = false;
  bool is_tab_created_by_gsa = false;
};

// A set of client states that should cover most of the actually relevant and
// practically occurring states. Automatically enumerating all possibilities is
// unfortunately not tractable due to the number of variable dimensions.
constexpr ClientState kRelevantClientStates[] = {
    // CCT, signed-in user, msbb enabled.
    {.is_custom_tab = true,
     .is_logged_in = true,
     .msbb_enabled = true,
     .is_supervised_user = false,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = true},

    // CCT, non-signed-in user, msbb enabled.
    {.is_custom_tab = true,
     .is_logged_in = false,
     .msbb_enabled = true,
     .is_supervised_user = false,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = true},

    // CCT, signed-in user, msbb disabled.
    {.is_custom_tab = true,
     .is_logged_in = true,
     .msbb_enabled = false,
     .is_supervised_user = false,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = true},

    // CCT, non-signed-in user, msbb disabled.
    {.is_custom_tab = true,
     .is_logged_in = false,
     .msbb_enabled = false,
     .is_supervised_user = false,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = true},

    // CCT, supervised user
    {.is_custom_tab = true,
     .msbb_enabled = true,
     .is_supervised_user = true,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = true},

    // CCT, tab not created by gsa
    {.is_custom_tab = true,
     .msbb_enabled = true,
     .is_supervised_user = false,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = false},

    // CCT, proactive help disabled
    {.is_custom_tab = true,
     .msbb_enabled = true,
     .is_supervised_user = false,
     .proactive_help_enabled = false,
     .is_tab_created_by_gsa = true},

    // Regular tab, signed-in user, msbb enabled
    {.is_custom_tab = false,
     .is_logged_in = true,
     .msbb_enabled = true,
     .is_supervised_user = false,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = true},

    // Regular tab, non-signed-in user, msbb enabled
    {.is_custom_tab = false,
     .is_logged_in = false,
     .msbb_enabled = true,
     .is_supervised_user = false,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = true},

    // Regular tab, signed-in user, msbb disabled
    {.is_custom_tab = false,
     .is_logged_in = true,
     .msbb_enabled = false,
     .is_supervised_user = false,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = true},

    // Regular tab, non-signed-in user, msbb disabled
    {.is_custom_tab = false,
     .is_logged_in = false,
     .msbb_enabled = false,
     .is_supervised_user = false,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = true},

    // Regular tab, is_tab_created_by_gsa=true
    {.is_custom_tab = false,
     .msbb_enabled = true,
     .is_supervised_user = false,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = true},

    // Regular tab, is_tab_created_by_gsa=false
    {.is_custom_tab = false,
     .msbb_enabled = true,
     .is_supervised_user = false,
     .proactive_help_enabled = true,
     .is_tab_created_by_gsa = false},

    // Regular tab, no msbb
    {.is_custom_tab = true,
     .msbb_enabled = false,
     .is_supervised_user = false,
     .proactive_help_enabled = true},

    // Regular tab, supervised user
    {.is_custom_tab = false,
     .msbb_enabled = true,
     .is_supervised_user = true,
     .proactive_help_enabled = true},

    // Regular tab, proactive help disabled
    {.is_custom_tab = false,
     .msbb_enabled = true,
     .is_supervised_user = false,
     .proactive_help_enabled = false},

    // Weblayer
    {.is_custom_tab = false,
     .is_weblayer = true,
     .msbb_enabled = true,
     .is_supervised_user = false,
     .proactive_help_enabled = true},

    // Weblayer, no msbb
    {.is_custom_tab = true,
     .is_weblayer = true,
     .msbb_enabled = false,
     .is_supervised_user = false,
     .proactive_help_enabled = true},

    // Weblayer, supervised user
    {.is_custom_tab = false,
     .is_weblayer = true,
     .msbb_enabled = true,
     .is_supervised_user = true,
     .proactive_help_enabled = true},

    // Weblayer, proactive help disabled
    {.is_custom_tab = false,
     .is_weblayer = true,
     .msbb_enabled = true,
     .is_supervised_user = false,
     .proactive_help_enabled = false},
};

inline void ApplyClientState(FakeStarterPlatformDelegate* platform_delegate,
                             const ClientState& client_state) {
  platform_delegate->is_custom_tab_ = client_state.is_custom_tab;
  platform_delegate->is_web_layer_ = client_state.is_weblayer;
  platform_delegate->is_logged_in_ = client_state.is_logged_in;
  platform_delegate->fake_common_dependencies_->msbb_enabled_ =
      client_state.msbb_enabled;
  platform_delegate->is_supervised_user_ = client_state.is_supervised_user;
  platform_delegate->proactive_help_enabled_ =
      client_state.proactive_help_enabled;
  platform_delegate->is_tab_created_by_gsa_ =
      client_state.is_tab_created_by_gsa;
}

}  // namespace starter_heuristic_configs_test_util
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_CONFIGS_STARTER_HEURISTIC_CONFIGS_TEST_UTIL_H_
