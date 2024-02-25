// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/constants.h"

#include "base/files/file_util.h"
#include "build/chromeos_buildflags.h"

#define ASSISTANT_DIR_STRING "google-assistant-library"
#define ASSISTANT_SOCKETS_STRING "sockets"
#define ASSISTANT_TEMP_DIR "/tmp/libassistant/"
#define LIBASSISTANT_DLC_DIR "opt/google/chrome/"
#define LIBASSISTANT_V2_NAME "libassistant_v2.so"

namespace ash::libassistant {

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
const base::FilePath::CharType kAssistantBaseDirPath[] =
    FILE_PATH_LITERAL("/home/chronos/user/" ASSISTANT_DIR_STRING);

const base::FilePath::CharType kLibAssistantSocketPath[] =
    FILE_PATH_LITERAL("/run/libassistant");

const char kLibAssistantDlcRootPath[] =
    "/run/imageloader/assistant-dlc/package/root";

const base::FilePath::CharType kLibAssistantV2DlcPath[] =
    FILE_PATH_LITERAL(LIBASSISTANT_DLC_DIR LIBASSISTANT_V2_NAME);
#else
// Directory and files used in gLinux simulation.
const base::FilePath::CharType kAssistantBaseDirPath[] =
    FILE_PATH_LITERAL(ASSISTANT_TEMP_DIR ASSISTANT_DIR_STRING);

const base::FilePath::CharType kLibAssistantSocketPath[] =
    FILE_PATH_LITERAL(ASSISTANT_TEMP_DIR ASSISTANT_SOCKETS_STRING);

const char kLibAssistantDlcRootPath[] = "";

const base::FilePath::CharType kLibAssistantV2DlcPath[] =
    FILE_PATH_LITERAL(LIBASSISTANT_V2_NAME);
#endif

}  // namespace ash::libassistant
