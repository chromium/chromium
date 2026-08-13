// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_input_adapter_factory.h"

#include "build/build_config.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_input_adapter_mac.h"
#else
#include "chrome/browser/ui/tabs/tab_drag_api/desktop_tab_drag_impl/tab_drag_session_input_adapter_aura.h"
#endif

namespace tabs_api {

// static
std::unique_ptr<TabDragSessionInputAdapter>
TabDragSessionInputAdapterFactory::Create() {
#if BUILDFLAG(IS_MAC)
  return std::make_unique<MacInputAdapter>();
#else
  return std::make_unique<AuraInputAdapter>();
#endif
}

}  // namespace tabs_api
