// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/constants.h"

#include "base/files/file_util.h"

#define ASSISTANT_DIR_STRING "google-assistant-library"

namespace chromeos {
namespace libassistant {

const base::FilePath::CharType kAssistantBaseDirPath[] =
    FILE_PATH_LITERAL("/home/chronos/user/" ASSISTANT_DIR_STRING);

const base::FilePath::CharType kAssistantTempBaseDirPath[] =
    FILE_PATH_LITERAL("/tmp/" ASSISTANT_DIR_STRING);

}  // namespace libassistant
}  // namespace chromeos
