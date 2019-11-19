// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRERENDER_MESSAGES_H_
#define CHROME_COMMON_PRERENDER_MESSAGES_H_

#include <stdint.h>

#include "chrome/common/prerender_types.h"
#include "content/public/common/referrer.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"
#include "url/origin.h"

#define IPC_MESSAGE_START PrerenderMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(prerender::PrerenderMode,
                          prerender::PRERENDER_MODE_COUNT - 1)

// PrerenderLinkManager Messages
// These are messages sent from the renderer to the browser in
// relation to <link rel=prerender> elements.

IPC_STRUCT_BEGIN(PrerenderAttributes)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(uint32_t, rel_types)
IPC_STRUCT_END()

// Notifies of the insertion of a <link rel=prerender> element in the
// document.
IPC_MESSAGE_CONTROL(PrerenderHostMsg_AddLinkRelPrerender,
                    int /* prerender_id, assigned by WebPrerendererClient */,
                    PrerenderAttributes,
                    content::Referrer,
                    url::Origin /* initiator_origin */,
                    gfx::Size,
                    int /* render_view_route_id of launcher */)

// Notifies on removal of a <link rel=prerender> element from the document.
IPC_MESSAGE_CONTROL1(PrerenderHostMsg_CancelLinkRelPrerender,
                     int /* prerender_id, assigned by WebPrerendererClient */)

// Notifies on unloading a <link rel=prerender> element from a frame.
IPC_MESSAGE_CONTROL1(PrerenderHostMsg_AbandonLinkRelPrerender,
                     int /* prerender_id, assigned by WebPrerendererClient */)

// Sent by the renderer process to notify that the resource prefetcher has
// discovered all possible subresources and issued requests for them.
IPC_MESSAGE_CONTROL0(PrerenderHostMsg_PrefetchFinished)

// PrerenderDispatcher Messages
// These are messages sent from the browser to the renderer in relation to
// running prerenders.

// Tells a renderer if it's currently being prerendered.  Must only be set
// before any navigation occurs, and only set to NO_PRERENDER at most once after
// that.
IPC_MESSAGE_ROUTED2(PrerenderMsg_SetIsPrerendering,
                    prerender::PrerenderMode,
                    std::string /* histogram_prefix */)

#endif  // CHROME_COMMON_PRERENDER_MESSAGES_H_
