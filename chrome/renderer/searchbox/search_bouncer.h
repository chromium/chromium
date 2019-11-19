// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCH_BOUNCER_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCH_BOUNCER_H_

#include "base/macros.h"
#include "chrome/common/search.mojom.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "url/gurl.h"

// SearchBouncer tracks a set of URLs which should be transferred back to the
// browser process for potential reassignment to an Instant renderer process.
class SearchBouncer : public content::RenderThreadObserver,
                      public chrome::mojom::SearchBouncer {
 public:
  SearchBouncer();
  ~SearchBouncer() override;

  static SearchBouncer* GetInstance();

  // RenderThreadObserver:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // Returns whether |url| is a valid Instant new tab page URL.
  bool IsNewTabPage(const GURL& url) const;

  // chrome::mojom::SearchBouncer:
  void SetNewTabPageURL(const GURL& new_tab_page_url) override;

 private:
  void BindSearchBouncerReceiver(
      mojo::PendingAssociatedReceiver<chrome::mojom::SearchBouncer> receiver);

  GURL new_tab_page_url_;

  mojo::AssociatedReceiver<chrome::mojom::SearchBouncer>
      search_bouncer_receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchBouncer);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCH_BOUNCER_H_
