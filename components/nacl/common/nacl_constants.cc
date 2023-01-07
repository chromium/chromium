// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_constants.h"

#include "base/files/file_path.h"

namespace nacl {

const char kNaClPluginName[] = "Native Client";

const char kNaClPluginMimeType[] = "application/x-nacl";
const char kNaClPluginExtension[] = "";
const char kNaClPluginDescription[] = "Native Client Executable";

const char kPnaclPluginMimeType[] = "application/x-pnacl";
const char kPnaclPluginExtension[] = "";
const char kPnaclPluginDescription[] = "Portable Native Client Executable";

const base::FilePath::CharType kInternalNaClPluginFileName[] =
    FILE_PATH_LITERAL("internal-nacl-plugin");

}  // namespace nacl
