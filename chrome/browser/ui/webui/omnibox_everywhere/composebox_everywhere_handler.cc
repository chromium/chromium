// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/composebox_everywhere_handler.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace {

class ComposeboxEverywhereClient final : public ComposeboxOmniboxClient {
 public:
  ComposeboxEverywhereClient(Profile* profile,
                             content::WebContents* web_contents,
                             ComposeboxEverywhereHandler* composebox_handler)
      : ComposeboxOmniboxClient(profile, web_contents, composebox_handler) {}

  ~ComposeboxEverywhereClient() override = default;

  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override {
    return metrics::OmniboxEventProto::COMPOSEBOX_EVERYWHERE;
  }
};

}  // namespace

ComposeboxEverywhereHandler::ComposeboxEverywhereHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    Profile* profile,
    content::WebContents* web_contents,
    GetSessionHandleCallback get_session_callback,
    ClearSessionHandleCallback clear_session_callback)
    : ComposeboxHandler(
          std::move(pending_handler),
          std::move(pending_page),
          std::move(pending_searchbox_handler),
          std::move(pending_searchbox_page),
          profile,
          web_contents,
          std::make_unique<ComposeboxEverywhereClient>(profile,
                                                       web_contents,
                                                       this),
          std::move(get_session_callback),
          std::move(clear_session_callback)) {}

ComposeboxEverywhereHandler::~ComposeboxEverywhereHandler() = default;
