// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_SWITCHES_H_
#define COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_SWITCHES_H_

// Defines all the command-line switches used by the encryptor component.

#include "base/component_export.h"
#include "build/build_config.h"

namespace os_crypt {
namespace switches {

#if BUILDFLAG(IS_APPLE)

// Uses mock keychain for testing purposes, which prevents blocking dialogs
// from causing timeouts.
COMPONENT_EXPORT(OS_CRYPT) extern const char kUseMockKeychain[];

#endif  // BUILDFLAG(IS_APPLE)

}  // namespace switches
}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_SYNC_OS_CRYPT_SWITCHES_H_
