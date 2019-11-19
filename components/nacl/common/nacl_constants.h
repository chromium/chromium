// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_COMMON_NACL_CONSTANTS_H_
#define COMPONENTS_NACL_COMMON_NACL_CONSTANTS_H_

#include "base/files/file_path.h"

namespace nacl {

extern const char kNaClPluginName[];
extern const char kNaClPluginMimeType[];
extern const char kNaClPluginExtension[];
extern const char kNaClPluginDescription[];

extern const char kPnaclPluginMimeType[];
extern const char kPnaclPluginExtension[];
extern const char kPnaclPluginDescription[];

extern const base::FilePath::CharType kInternalNaClPluginFileName[];

}  // namespace nacl

#endif  // COMPONENTS_NACL_COMMON_NACL_CONSTANTS_H_
