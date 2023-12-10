// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spelling_engine.h"

#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
#include "components/spellcheck/renderer/hunspell_engine.h"
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#include "components/spellcheck/renderer/platform_spelling_engine.h"
#endif

SpellingEngine* CreateNativeSpellingEngine(
    service_manager::LocalInterfaceProvider* embedder_provider) {
  DCHECK(embedder_provider);
#if BUILDFLAG(IS_WIN)
  // On Windows, always return a HunspellEngine. This is a simplification to
  // avoid needing an async Mojo call to the browser process to determine which
  // languages are supported by the native spell checker. Returning a
  // HunspellEngine for languages supported by the native spell checker is
  // harmless because the SpellingEngine object returned here is never used
  // during native spell checking. It also doesn't affect Hunspell, since these
  // languages are skipped during the Hunspell check.
  return new HunspellEngine(embedder_provider);
#elif BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  return new PlatformSpellingEngine();
#elif BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  return new HunspellEngine(embedder_provider);
#else
  return nullptr;
#endif
}
