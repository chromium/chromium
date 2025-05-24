// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"

#include "ui/base/interaction/state_observer.h"

DEFINE_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                              kPageActionButtonVisible);
