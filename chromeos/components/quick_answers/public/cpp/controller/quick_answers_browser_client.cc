// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_browser_client.h"

#include "base/check_op.h"

namespace ash {

namespace {
QuickAnswersBrowserClient* g_instance = nullptr;
}

QuickAnswersBrowserClient::QuickAnswersBrowserClient() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

QuickAnswersBrowserClient::~QuickAnswersBrowserClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

QuickAnswersBrowserClient* QuickAnswersBrowserClient::Get() {
  return g_instance;
}
}  // namespace ash
