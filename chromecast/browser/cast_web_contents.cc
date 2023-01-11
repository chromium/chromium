// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_contents.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace chromecast {

CastWebContents::CastWebContents() = default;

CastWebContents::~CastWebContents() = default;

void CastWebContents::BindOwnerReceiver(
    mojo::PendingReceiver<mojom::CastWebContents> receiver) {
  DCHECK(!owner_receiver_.is_bound());
  owner_receiver_.Bind(std::move(receiver));
  owner_receiver_.set_disconnect_handler(
      base::BindOnce(&CastWebContents::OnDisconnect, base::Unretained(this)));
}

void CastWebContents::BindSharedReceiver(
    mojo::PendingReceiver<mojom::CastWebContents> receiver) {
  shared_receivers_.Add(this, std::move(receiver));
}

void CastWebContents::AddObserver(
    mojo::PendingRemote<mojom::CastWebContentsObserver> observer) {
  observers_.Add(std::move(observer));
}

void CastWebContents::AddObserver(CastWebContents::Observer* observer) {
  DCHECK(observer);
  sync_observers_.AddObserver(observer);
}

void CastWebContents::RemoveObserver(CastWebContents::Observer* observer) {
  DCHECK(observer);
  sync_observers_.RemoveObserver(observer);
}

void CastWebContents::SetDisconnectCallback(base::OnceClosure cb) {
  disconnect_cb_ = std::move(cb);
}

void CastWebContents::OnDisconnect() {
  if (disconnect_cb_) {
    std::move(disconnect_cb_).Run();
  }
}

CastWebContents::Observer::Observer() : cast_web_contents_(nullptr) {}

CastWebContents::Observer::~Observer() {
  Observe(nullptr);
}

void CastWebContents::Observer::Observe(CastWebContents* cast_web_contents) {
  if (cast_web_contents == cast_web_contents_) {
    // Early exit to avoid infinite loops if we're in the middle of a callback.
    return;
  }
  if (cast_web_contents_) {
    cast_web_contents_->RemoveObserver(this);
  }
  cast_web_contents_ = cast_web_contents;
  if (cast_web_contents_) {
    cast_web_contents_->AddObserver(this);
  }
}

void CastWebContents::Observer::ResetCastWebContents() {
  cast_web_contents_ = nullptr;
}

}  // namespace chromecast
