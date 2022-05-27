// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill_assistant/password_change/apc_utils.h"

#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"

const gfx::VectorIcon& GetAssistantIconOrFallback() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return kAssistantIcon;
#else
  // Only developer builds will ever use this branch.
  return kProductIcon;
#endif
}
