// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"

#include "base/check.h"
#include "base/logging.h"

namespace {
chromeos::MagicBoostState* g_magic_boost_state = nullptr;
}

namespace chromeos {

// static
MagicBoostState* MagicBoostState::Get() {
  return g_magic_boost_state;
}

MagicBoostState::MagicBoostState() {
  CHECK(!g_magic_boost_state);
  g_magic_boost_state = this;
}

MagicBoostState::~MagicBoostState() {
  CHECK_EQ(g_magic_boost_state, this);
  g_magic_boost_state = nullptr;
}

void MagicBoostState::AddObserver(MagicBoostState::Observer* observer) {
  observers_.AddObserver(observer);
}

void MagicBoostState::RemoveObserver(MagicBoostState::Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace chromeos
