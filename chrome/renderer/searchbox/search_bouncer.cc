// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/search_bouncer.h"

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace {
base::LazyInstance<SearchBouncer>::Leaky g_search_bouncer =
    LAZY_INSTANCE_INITIALIZER;

GURL RemoveQueryAndRef(const GURL& url) {
  url::Replacements<char> replacements;
  replacements.ClearQuery();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

}  // namespace

SearchBouncer::SearchBouncer() = default;

SearchBouncer::~SearchBouncer() = default;

// static
SearchBouncer* SearchBouncer::GetInstance() {
  return g_search_bouncer.Pointer();
}

void SearchBouncer::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  // Note: Unretained is safe here because this class is a leaky LazyInstance.
  // For the same reason, UnregisterMojoInterfaces isn't required.
  associated_interfaces->AddInterface(base::Bind(
      &SearchBouncer::BindSearchBouncerReceiver, base::Unretained(this)));
}

bool SearchBouncer::IsNewTabPage(const GURL& url) const {
  GURL url_no_query_or_ref = RemoveQueryAndRef(url);
  return url_no_query_or_ref.is_valid() &&
         url_no_query_or_ref == new_tab_page_url_;
}

void SearchBouncer::SetNewTabPageURL(const GURL& new_tab_page_url) {
  new_tab_page_url_ = new_tab_page_url;
}

void SearchBouncer::BindSearchBouncerReceiver(
    mojo::PendingAssociatedReceiver<chrome::mojom::SearchBouncer> receiver) {
  search_bouncer_receiver_.Bind(std::move(receiver));
}
