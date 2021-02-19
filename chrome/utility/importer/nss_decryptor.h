// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_H_
#define CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include "chrome/utility/importer/nss_decryptor_win.h"
#elif defined(USE_NSS_CERTS)
#include "chrome/utility/importer/nss_decryptor_system_nss.h"
#else
#error NSSDecryptor not implemented.
#endif

#endif  // CHROME_UTILITY_IMPORTER_NSS_DECRYPTOR_H_
