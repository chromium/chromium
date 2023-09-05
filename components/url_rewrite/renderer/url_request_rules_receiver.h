// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_REWRITE_RENDERER_URL_REQUEST_RULES_RECEIVER_H_
#define COMPONENTS_URL_REWRITE_RENDERER_URL_REQUEST_RULES_RECEIVER_H_

#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "components/url_rewrite/common/url_request_rewrite_rules.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace url_rewrite {

// Provides rewriting rules for network requests. This object must be
// destroyed on RenderFrame destruction. This class should only be used on the
// IO thread.
class UrlRequestRulesReceiver : public mojom::UrlRequestRulesReceiver {
 public:
  explicit UrlRequestRulesReceiver(content::RenderFrame* render_frame);
  ~UrlRequestRulesReceiver() override;

  UrlRequestRulesReceiver(const UrlRequestRulesReceiver&) = delete;
  UrlRequestRulesReceiver& operator=(const UrlRequestRulesReceiver&) = delete;

  // Returns cached rules. This method may be called from multiple threads (eg
  // Blink main and worker threads), hence the method returns the scoped_refptr
  // by value, so that the refptr is still valid if changed on the other thread.
  //
  // Note: The object wrapped by refptr is guaranteed to never change making it
  // safe for read access across the threads.
  //
  // Note: The API should be called every time the rewrite rules are requested
  // to make sure the callers get the most up to date rules. The refptr should
  // not be cached by callers!
  scoped_refptr<UrlRequestRewriteRules> GetCachedRules() const;

 private:
  void OnUrlRequestRulesReceiverAssociatedReceiver(
      mojo::PendingAssociatedReceiver<mojom::UrlRequestRulesReceiver> receiver);

  // mojom::UrlRequestRulesReceiver implementation.
  void OnRulesUpdated(mojom::UrlRequestRewriteRulesPtr rules) override;

  mutable base::Lock lock_;
  scoped_refptr<UrlRequestRewriteRules> cached_rules_ GUARDED_BY(lock_);
  mojo::AssociatedReceiver<mojom::UrlRequestRulesReceiver>
      url_request_rules_receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace url_rewrite

#endif  // COMPONENTS_URL_REWRITE_RENDERER_URL_REQUEST_RULES_RECEIVER_H_
