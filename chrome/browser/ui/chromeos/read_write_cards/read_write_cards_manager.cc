// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_manager.h"

#include "base/check_op.h"

namespace chromeos {

namespace {

ReadWriteCardsManager* g_instance = nullptr;

}  // namespace

ReadWriteCardsManager::ReadWriteCardsManager() {
  CHECK(!g_instance);
  g_instance = this;
}

ReadWriteCardsManager::~ReadWriteCardsManager() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
ReadWriteCardsManager* ReadWriteCardsManager::Get() {
  return g_instance;
}

}  // namespace chromeos
