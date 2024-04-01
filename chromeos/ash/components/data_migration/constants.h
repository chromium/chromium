// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_CONSTANTS_H_

#include "base/files/file_path.h"

namespace data_migration {

// Used by nearby connections to differentiate data migration from other nearby
// use cases.
//
// NC recommends using your Android app's package name for the service id. Data
// migration does not have an Android app, but this identifier matches the
// Android package naming conventions and is unique within the ecosystem.
inline constexpr char kServiceId[] = "com.google.chromeos.migration";

// Temporary location in the user's home directory where the payloads
// transferred from the remote device will be stored.
// TODO(esum): Remove this and write the file to the real directory. This is
// just being used for testing right now.
inline constexpr base::FilePath::CharType kPayloadTargetDir[] =
    FILE_PATH_LITERAL("MyFiles/Downloads");

}  // namespace data_migration

#endif  // CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_CONSTANTS_H_
