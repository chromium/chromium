// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_LOCAL_AUTHENTICATION_TOKEN_H_
#define CHROME_BROWSER_WEBAUTHN_LOCAL_AUTHENTICATION_TOKEN_H_

#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_MAC)
#include "crypto/apple/scoped_lacontext.h"
#endif

namespace webauthn {

#if BUILDFLAG(IS_MAC)
using LocalAuthenticationToken = crypto::apple::ScopedLAContext;
#else
struct NoopLocalAuthenticationToken {};
using LocalAuthenticationToken = NoopLocalAuthenticationToken;
#endif

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_LOCAL_AUTHENTICATION_TOKEN_H_
