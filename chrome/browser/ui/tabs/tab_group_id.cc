// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_id.h"

// static
TabGroupId TabGroupId::GenerateNew() {
  return TabGroupId(base::Token::CreateRandom());
}

// static
TabGroupId TabGroupId::FromRawToken(base::Token token) {
  return TabGroupId(token);
}

TabGroupId::TabGroupId(const TabGroupId& other) = default;

TabGroupId& TabGroupId::operator=(const TabGroupId& other) = default;

bool TabGroupId::operator==(const TabGroupId& other) const {
  return token_ == other.token_;
}

bool TabGroupId::operator!=(const TabGroupId& other) const {
  return !(*this == other);
}

bool TabGroupId::operator<(const TabGroupId& other) const {
  return token_ < other.token_;
}

std::string TabGroupId::ToString() const {
  return token_.ToString();
}

TabGroupId::TabGroupId(base::Token token) : token_(token) {}
