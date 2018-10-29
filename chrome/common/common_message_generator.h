// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.

#include "chrome/common/common_param_traits_macros.h"
#include "chrome/common/instant_struct_traits.h"
#include "services/network/public/cpp/p2p_param_traits.h"
#undef CHROME_COMMON_MAC_APP_SHIM_PARAM_TRAITS_H_
#include "chrome/common/mac/app_shim_param_traits.h"
#ifndef CHROME_COMMON_MAC_APP_SHIM_PARAM_TRAITS_H_
#error "Failed to include header chrome/common/mac/app_shim_param_traits.h"
#endif
#undef CHROME_COMMON_PRERENDER_MESSAGES_H_
#include "chrome/common/prerender_messages.h"
#ifndef CHROME_COMMON_PRERENDER_MESSAGES_H_
#error "Failed to include header chrome/common/prerender_messages.h"
#endif
#undef CHROME_COMMON_RENDER_MESSAGES_H_
#include "chrome/common/render_messages.h"
#ifndef CHROME_COMMON_RENDER_MESSAGES_H_
#error "Failed to include header chrome/common/render_messages.h"
#endif
#undef CHROME_COMMON_TTS_MESSAGES_H_
#include "chrome/common/tts_messages.h"
#ifndef CHROME_COMMON_TTS_MESSAGES_H_
#error "Failed to include header chrome/common/tts_messages.h"
#endif
#include "content/public/common/common_param_traits.h"
#include "content/public/common/common_param_traits_macros.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#undef CHROME_COMMON_CAST_MESSAGES_H_
#include "chrome/common/cast_messages.h"
#ifndef CHROME_COMMON_CAST_MESSAGES_H_
#error "Failed to include header chrome/common/cast_messages.h"
#endif
#undef CHROME_COMMON_EXTENSIONS_CHROME_EXTENSION_MESSAGES_H_
#include "chrome/common/extensions/chrome_extension_messages.h"
#ifndef CHROME_COMMON_EXTENSIONS_CHROME_EXTENSION_MESSAGES_H_
#error "Failed to include chrome/common/extensions/chrome_extension_messages.h"
#endif
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#undef CHROME_COMMON_CHROME_UTILITY_PRINTING_MESSAGES_H_
#include "chrome/common/chrome_utility_printing_messages.h"
#ifndef CHROME_COMMON_CHROME_UTILITY_PRINTING_MESSAGES_H_
#error \
    "Failed to include header chrome/common/chrome_utility_printing_messages.h"
#endif
#endif

#undef CHROME_COMMON_MEDIA_WEBRTC_LOGGING_MESSAGES_H_
#include "chrome/common/media/webrtc_logging_messages.h"
#ifndef CHROME_COMMON_MEDIA_WEBRTC_LOGGING_MESSAGES_H_
#error "Failed to include header chrome/common/media/webrtc_logging_messages.h"
#endif

#if defined(FULL_SAFE_BROWSING)
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer_param_traits.h"
#endif
