// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CRYPTUIAPI_SHIM_H_
#define CHROME_BROWSER_UI_CRYPTUIAPI_SHIM_H_

// cryptuiapi.h includes wincrypt.h which defines macros which conflict with
// OpenSSL's types. This header includes cryptuiapi.h and then wincrypt_shim.h
// which undefines the OpenSSL macros which conflict. Any Chromium headers
// which want to include cryptuiapi should instead include this header.

#include <cryptuiapi.h>
#include <windows.h>

#include "base/win/wincrypt_shim.h"

#endif  // CHROME_BROWSER_UI_CRYPTUIAPI_SHIM_H_
