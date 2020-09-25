// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_constants.h"

namespace federated_learning {

const char kManifestBlocklistFormatKey[] = "blocklist_format";

const int kCurrentBlocklistFormatVersion = 1;

const base::FilePath::CharType kTopLevelDirectoryName[] =
    FILE_PATH_LITERAL("Floc");

const base::FilePath::CharType kBlocklistBaseDirectoryName[] =
    FILE_PATH_LITERAL("Blocklist");

const base::FilePath::CharType kBlocklistFileName[] =
    FILE_PATH_LITERAL("Blocklist");

}  // namespace federated_learning
