// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"

#include "base/check_op.h"

namespace {
QuickAnswersController* g_instance = nullptr;
}

QuickAnswersController::QuickAnswersController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

QuickAnswersController::~QuickAnswersController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

QuickAnswersController* QuickAnswersController::Get() {
  return g_instance;
}
