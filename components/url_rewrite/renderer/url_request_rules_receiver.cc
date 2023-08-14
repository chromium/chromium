// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_rewrite/renderer/url_request_rules_receiver.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace url_rewrite {

UrlRequestRulesReceiver::UrlRequestRulesReceiver(
    content::RenderFrame* render_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(render_frame);

  // It is fine to use an unretained pointer to |this| here as the
  // AssociatedInterfaceRegistry, owned by |render_frame| will be torn-down at
  // the same time as |this|.
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::UrlRequestRulesReceiver>(base::BindRepeating(
          &UrlRequestRulesReceiver::OnUrlRequestRulesReceiverAssociatedReceiver,
          base::Unretained(this)));
}

UrlRequestRulesReceiver::~UrlRequestRulesReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<UrlRequestRewriteRules> UrlRequestRulesReceiver::GetCachedRules()
    const {
  base::AutoLock guard(lock_);
  return cached_rules_;
}

void UrlRequestRulesReceiver::OnUrlRequestRulesReceiverAssociatedReceiver(
    mojo::PendingAssociatedReceiver<mojom::UrlRequestRulesReceiver> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!url_request_rules_receiver_.is_bound());
  url_request_rules_receiver_.Bind(std::move(receiver));
}

void UrlRequestRulesReceiver::OnRulesUpdated(
    mojom::UrlRequestRewriteRulesPtr rules) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLock guard(lock_);
  cached_rules_ =
      base::MakeRefCounted<UrlRequestRewriteRules>(std::move(rules));
}

}  // namespace url_rewrite
