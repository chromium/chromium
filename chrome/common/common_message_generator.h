// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.

#include "chrome/common/search/instant_mojom_traits.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/common_param_traits_macros.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "services/network/public/cpp/p2p_param_traits.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer_param_traits.h"
#endif
