// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_dispatcher.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/prerender_types.h"
#include "chrome/renderer/prerender/prerender_extra_data.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_prerendering_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace prerender {

using blink::WebPrerender;
using blink::WebPrerenderingSupport;

PrerenderDispatcher::PrerenderDispatcher()
    : process_start_time_(base::TimeTicks::Now()) {
  WebPrerenderingSupport::Initialize(this);
}

PrerenderDispatcher::~PrerenderDispatcher() {
  WebPrerenderingSupport::Shutdown();
}

bool PrerenderDispatcher::IsPrerenderURL(const GURL& url) const {
  return running_prerender_urls_.count(url) >= 1;
}

void PrerenderDispatcher::IncrementPrefetchCount() {
  prefetch_count_++;
}

void PrerenderDispatcher::DecrementPrefetchCount() {
  if (!--prefetch_count_ && prefetch_finished_) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Prerender.NoStatePrefetchRendererLifetimeExtension",
        base::TimeTicks::Now() - prefetch_parsed_time_);
    content::RenderThread::Get()->Send(new PrerenderHostMsg_PrefetchFinished());
  }
}

void PrerenderDispatcher::PrerenderStart(int prerender_id) {
  auto it = prerenders_.find(prerender_id);
  if (it == prerenders_.end())
    return;

  WebPrerender& prerender = it->second;

  // The prerender should only be null in unit tests.
  if (prerender.IsNull())
    return;

  prerender.DidStartPrerender();
}

void PrerenderDispatcher::PrerenderStopLoading(int prerender_id) {
  auto it = prerenders_.find(prerender_id);
  if (it == prerenders_.end())
    return;

  WebPrerender& prerender = it->second;
  DCHECK(!prerender.IsNull())
      << "OnPrerenderStopLoading shouldn't be called from a unit test, the only"
      << "context in which a WebPrerender in the dispatcher can be null.";

  prerender.DidSendLoadForPrerender();
}

void PrerenderDispatcher::PrerenderDomContentLoaded(int prerender_id) {
  auto it = prerenders_.find(prerender_id);
  if (it == prerenders_.end())
    return;

  WebPrerender& prerender = it->second;
  DCHECK(!prerender.IsNull())
      << "OnPrerenderDomContentLoaded shouldn't be called from a unit test,"
      << " the only context in which a WebPrerender in the dispatcher can be"
      << " null.";

  prerender.DidSendDOMContentLoadedForPrerender();
}

void PrerenderDispatcher::PrerenderAddAlias(const GURL& alias) {
  running_prerender_urls_.insert(alias);
}

void PrerenderDispatcher::PrerenderRemoveAliases(
    const std::vector<GURL>& aliases) {
  for (size_t i = 0; i < aliases.size(); ++i) {
    auto it = running_prerender_urls_.find(aliases[i]);
    if (it != running_prerender_urls_.end()) {
      running_prerender_urls_.erase(it);
    }
  }
}

void PrerenderDispatcher::PrerenderStop(int prerender_id) {
  auto it = prerenders_.find(prerender_id);
  if (it == prerenders_.end())
    return;
  WebPrerender& prerender = it->second;

  // The prerender should only be null in unit tests.
  if (!prerender.IsNull())
    prerender.DidStopPrerender();

  // TODO(cbentzel): We'd also want to send the map of active prerenders when
  // creating a new render process, so the Add/Remove go relative to that.
  // This may not be that big of a deal in practice, since the newly created tab
  // is unlikely to go to the prerendered page.
  prerenders_.erase(prerender_id);
}

void PrerenderDispatcher::OnPrerenderDispatcherRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::PrerenderDispatcher>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PrerenderDispatcher::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface(
      base::Bind(&PrerenderDispatcher::OnPrerenderDispatcherRequest,
                 base::Unretained(this)));
}

void PrerenderDispatcher::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(
      chrome::mojom::PrerenderDispatcher::Name_);
}

void PrerenderDispatcher::Add(const WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  if (prerenders_.count(extra_data.prerender_id()) != 0) {
    // TODO(gavinp): Determine why these apparently duplicate adds occur.
    return;
  }

  prerenders_[extra_data.prerender_id()] = prerender;

  PrerenderAttributes attributes;
  attributes.url = GURL(prerender.Url());
  attributes.rel_types = prerender.RelTypes();

  content::RenderThread::Get()->Send(new PrerenderHostMsg_AddLinkRelPrerender(
      extra_data.prerender_id(), attributes,
      content::Referrer::SanitizeForRequest(
          GURL(prerender.Url()),
          content::Referrer(blink::WebStringToGURL(prerender.GetReferrer()),
                            prerender.GetReferrerPolicy())),
      prerender.SecurityOrigin(), extra_data.size(),
      extra_data.render_view_route_id()));
}

void PrerenderDispatcher::Cancel(const WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  content::RenderThread::Get()->Send(
      new PrerenderHostMsg_CancelLinkRelPrerender(extra_data.prerender_id()));
  // The browser will not send an OnPrerenderStop (the prerender may have even
  // been canceled before it was started), so release it to avoid a
  // leak. Moreover, if it did, the PrerenderClient in Blink will have been
  // detached already.
  prerenders_.erase(extra_data.prerender_id());
}

void PrerenderDispatcher::Abandon(const WebPrerender& prerender) {
  const PrerenderExtraData& extra_data =
      PrerenderExtraData::FromPrerender(prerender);
  content::RenderThread::Get()->Send(
      new PrerenderHostMsg_AbandonLinkRelPrerender(extra_data.prerender_id()));
  // The browser will not send an OnPrerenderStop (the prerender may have even
  // been canceled before it was started), so release it to avoid a
  // leak. Moreover, if it did, the PrerenderClient in Blink will have been
  // detached already.
  prerenders_.erase(extra_data.prerender_id());
}

void PrerenderDispatcher::PrefetchFinished() {
  prefetch_parsed_time_ = base::TimeTicks::Now();
  if (prefetch_count_) {
    prefetch_finished_ = true;
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Prerender.NoStatePrefetchRendererParseTime",
        prefetch_parsed_time_ - process_start_time_);
    content::RenderThread::Get()->Send(new PrerenderHostMsg_PrefetchFinished());
  }
}

}  // namespace prerender
