// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_manager_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_features.h"

namespace compose {

ComposeManagerImpl::ComposeManagerImpl(ComposeClient* client)
    : client_(*client) {}

ComposeManagerImpl::~ComposeManagerImpl() = default;

bool ComposeManagerImpl::IsEnabled() const {
  return base::FeatureList::IsEnabled(features::kEnableCompose);
}

void ComposeManagerImpl::OfferCompose(ComposeCallback callback) {
  callback_ = std::move(callback);
  CHECK(IsEnabled());
  // TODO(b/301609035): Either pass a weak pointer or make sure that
  // the dialog never outlives the tab. (Should be a given once the
  // bubble destroys itself prior to WebContents destruction.)
  client_->ShowComposeDialog(base::BindOnce(
      &ComposeManagerImpl::ComposeTextForQuery, base::Unretained(this)));
}

void ComposeManagerImpl::ComposeTextForQuery(
    const ComposeClient::QueryParams& params) {
  std::move(callback_).Run(u"Cucumbers? " + params.query);
}

}  // namespace compose
