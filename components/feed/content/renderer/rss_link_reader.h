// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CONTENT_RENDERER_RSS_LINK_READER_H_
#define COMPONENTS_FEED_CONTENT_RENDERER_RSS_LINK_READER_H_

#include "components/feed/mojom/rss_link_reader.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class RenderFrame;
}
namespace feed {

// Implements mojom::RssLinkReader, see rss_link_reader.mojom.
class RssLinkReader : public content::RenderFrameObserver,
                      public mojom::RssLinkReader {
 public:
  explicit RssLinkReader(content::RenderFrame* render_frame,
                         service_manager::BinderRegistry* registry);
  ~RssLinkReader() override;

  // mojom::RssLinkReader
  void GetRssLinks(GetRssLinksCallback callback) override;

  // content::RenderFrameObserver
  void OnDestruct() override;

 private:
  void BindReceiver(mojo::PendingReceiver<mojom::RssLinkReader> receiver);

  mojo::Receiver<mojom::RssLinkReader> receiver_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CONTENT_RENDERER_RSS_LINK_READER_H_
