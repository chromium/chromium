// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/strike_database_base.h"

namespace autofill {

StrikeDatabaseBase::StrikeDatabaseBase() = default;

StrikeDatabaseBase::~StrikeDatabaseBase() = default;

std::string StrikeDatabaseBase::KeyDeliminator() {
  return kKeyDeliminator;
}

}  // namespace autofill
