// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/appid.h"

#include "build/branding_buildflags.h"

namespace google_update {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const wchar_t kAppGuid[] = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const wchar_t kBetaAppGuid[] = L"{8237E44A-0054-442C-B6B6-EA0509993955}";
const wchar_t kDevAppGuid[] = L"{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}";
const wchar_t kSxSAppGuid[] = L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}";
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
const wchar_t kAppGuid[] = L"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace google_update
