// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_STDAFX_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_STDAFX_H_

// Include common system include files.

#include <windows.h>
#include <winternl.h>

#include <wincred.h>
#include <wincrypt.h>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <Shellapi.h>  // For CommandLineToArgvW()
#include <Shlobj.h>
#include <aclapi.h>
#include <credentialprovider.h>
#include <dpapi.h>
#include <lm.h>
#include <MDMRegistration.h>
#include <ntsecapi.h>
#include <propkey.h>
#include <sddl.h>
#include <security.h>
#include <userenv.h>
#include <versionhelpers.h>

#include <malloc.h>
#include <memory.h>
#include <stdlib.h>

#include <fcntl.h>  // for _O_TEXT | _O_APPEND
#include <inttypes.h>
#include <io.h>

#include <memory>

// The ATL headers don't like to be compiled with INITGUID defined.
#if !defined(INITGUID)

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some CString ctors are explicit
#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>

#endif

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_STDAFX_H_
