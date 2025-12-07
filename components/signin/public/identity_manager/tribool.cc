// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/tribool.h"

#include "base/check_op.h"

namespace signin {

Tribool TriboolFromBool(bool b) {
  return b ? Tribool::kTrue : Tribool::kFalse;
}

bool TriboolToBoolOrDie(Tribool tribool) {
  CHECK_NE(tribool, Tribool::kUnknown);
  return tribool == Tribool::kTrue;
}

bool TriboolToBoolOr(signin::Tribool tribool, bool default_value) {
  switch (tribool) {
    case signin::Tribool::kTrue:
      return true;
    case signin::Tribool::kFalse:
      return false;
    case signin::Tribool::kUnknown:
      return default_value;
  }
}

std::string TriboolToString(Tribool tribool) {
  switch (tribool) {
    case Tribool::kUnknown:
      return "Unknown";
    case Tribool::kFalse:
      return "False";
    case Tribool::kTrue:
      return "True";
  }
}

}  // namespace signin
