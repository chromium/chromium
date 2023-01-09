// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/browser_exposed_renderer_interfaces.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "chrome/renderer/media/webrtc_logging_agent_impl.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "mojo/public/cpp/bindings/binder_map.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "components/spellcheck/renderer/spellcheck.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/allocator/buildflags.h"
#if defined(ARCH_CPU_X86_64)
#include "chrome/renderer/performance_manager/mechanisms/userspace_swap_impl_chromeos.h"
#endif  // defined(ARCH_CPU_X86_64)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
#include "chrome/renderer/font_prewarmer.h"
#endif

namespace {

void BindWebRTCLoggingAgent(
    ChromeContentRendererClient* client,
    mojo::PendingReceiver<chrome::mojom::WebRtcLoggingAgent> receiver) {
  client->GetWebRtcLoggingAgent()->AddReceiver(std::move(receiver));
}

#if BUILDFLAG(ENABLE_SPELLCHECK)
void BindSpellChecker(
    ChromeContentRendererClient* client,
    mojo::PendingReceiver<spellcheck::mojom::SpellChecker> receiver) {
  if (client->GetSpellCheck())
    client->GetSpellCheck()->BindReceiver(std::move(receiver));
}
#endif

}  // namespace

void ExposeChromeRendererInterfacesToBrowser(
    ChromeContentRendererClient* client,
    mojo::BinderMap* binders) {
  binders->Add<visitedlink::mojom::VisitedLinkNotificationSink>(
      client->GetChromeObserver()->visited_link_reader()->GetBindCallback(),
      base::SequencedTaskRunner::GetCurrentDefault());

  binders->Add<web_cache::mojom::WebCache>(
      base::BindRepeating(&web_cache::WebCacheImpl::BindReceiver,
                          base::Unretained(client->GetWebCache())),
      base::SequencedTaskRunner::GetCurrentDefault());

  binders->Add<chrome::mojom::WebRtcLoggingAgent>(
      base::BindRepeating(&BindWebRTCLoggingAgent, client),
      base::SequencedTaskRunner::GetCurrentDefault());

#if BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(ARCH_CPU_X86_64)
  if (performance_manager::mechanism::UserspaceSwapImpl::
          PlatformSupportsUserspaceSwap()) {
    binders->Add<userspace_swap::mojom::UserspaceSwap>(
        base::BindRepeating(
            &performance_manager::mechanism::UserspaceSwapImpl::Create),
        base::SequencedTaskRunner::GetCurrentDefault());
  }
#endif  // defined(ARCH_CPU_X86_64)

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_SPELLCHECK)
  binders->Add<spellcheck::mojom::SpellChecker>(
      base::BindRepeating(&BindSpellChecker, client),
      base::SequencedTaskRunner::GetCurrentDefault());
#endif

#if BUILDFLAG(IS_WIN)
  binders->Add<chrome::mojom::FontPrewarmer>(
      base::BindRepeating(&FontPrewarmer::Bind),
      base::SequencedTaskRunner::GetCurrentDefault());
#endif
}
