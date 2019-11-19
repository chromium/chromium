// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_
#define CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "chrome/common/prerender.mojom.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_prerender.h"
#include "third_party/blink/public/platform/web_prerendering_support.h"

class GURL;

namespace prerender {

// There is one PrerenderDispatcher per render process. It keeps track of which
// prerenders were launched from this renderer, and ensures prerender navigation
// is triggered on navigation to those. It implements the prerendering interface
// supplied to WebKit.
class PrerenderDispatcher : public content::RenderThreadObserver,
                            public blink::WebPrerenderingSupport,
                            public chrome::mojom::PrerenderDispatcher {
 public:
  PrerenderDispatcher();
  ~PrerenderDispatcher() override;

  bool IsPrerenderURL(const GURL& url) const;

  void IncrementPrefetchCount();
  void DecrementPrefetchCount();

 private:
  friend class PrerenderDispatcherTest;

  // chrome::mojom::PrerenderDispatcher:
  void PrerenderStart(int prerender_id) override;
  void PrerenderStopLoading(int prerender_id) override;
  void PrerenderDomContentLoaded(int prerender_id) override;
  void PrerenderAddAlias(const GURL& alias) override;
  void PrerenderRemoveAliases(const std::vector<GURL>& aliases) override;
  void PrerenderStop(int prerender_id) override;

  void OnPrerenderDispatcherRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::PrerenderDispatcher>
          receiver);

  // From RenderThreadObserver:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // From WebPrerenderingSupport:
  void Add(const blink::WebPrerender& prerender) override;
  void Cancel(const blink::WebPrerender& prerender) override;
  void Abandon(const blink::WebPrerender& prerender) override;
  void PrefetchFinished() override;

  // From WebKit, prerender elements launched by renderers in our process.
  std::map<int, blink::WebPrerender> prerenders_;

  // From the browser process, which prerenders are running, indexed by URL.
  // Updated by the browser processes as aliases are discovered.
  std::multiset<GURL> running_prerender_urls_;

  int prefetch_count_ = 0;
  bool prefetch_finished_ = false;
  base::TimeTicks process_start_time_;
  base::TimeTicks prefetch_parsed_time_;

  mojo::AssociatedReceiverSet<chrome::mojom::PrerenderDispatcher> receivers_;
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDER_DISPATCHER_H_
