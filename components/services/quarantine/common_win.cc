// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/common_win.h"

namespace quarantine {

// [MS-FSCC] Section 5.6.1
const base::FilePath::CharType kZoneIdentifierStreamSuffix[] =
    FILE_PATH_LITERAL(":Zone.Identifier");

}  // namespace quarantine
