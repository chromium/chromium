// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/enterprise/enterprise_shortcut.h"

namespace ntp_tiles {
EnterpriseShortcut::EnterpriseShortcut() = default;

EnterpriseShortcut::EnterpriseShortcut(const EnterpriseShortcut&) = default;

EnterpriseShortcut::~EnterpriseShortcut() = default;

bool EnterpriseShortcut::operator==(const EnterpriseShortcut& other) const =
    default;

}  // namespace ntp_tiles
