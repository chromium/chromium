// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/browser_exposed_renderer_interfaces.h"

#include <memory>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
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
#if BUILDFLAG(USE_TCMALLOC)
#include "chrome/common/performance_manager/mojom/tcmalloc.mojom.h"
#include "chrome/renderer/performance_manager/mechanisms/tcmalloc_tunables_impl.h"
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
  binders->Add(
      client->GetChromeObserver()->visited_link_reader()->GetBindCallback(),
      base::SequencedTaskRunnerHandle::Get());

  binders->Add(base::BindRepeating(&web_cache::WebCacheImpl::BindReceiver,
                                   base::Unretained(client->GetWebCache())),
               base::SequencedTaskRunnerHandle::Get());

  binders->Add(base::BindRepeating(&BindWebRTCLoggingAgent, client),
               base::SequencedTaskRunnerHandle::Get());

#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(USE_TCMALLOC)
  binders->Add(
      base::BindRepeating(
          &performance_manager::mechanism::TcmallocTunablesImpl::Create),
      base::SequencedTaskRunnerHandle::Get());
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_SPELLCHECK)
  binders->Add(base::BindRepeating(&BindSpellChecker, client),
               base::SequencedTaskRunnerHandle::Get());
#endif
}
