// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/extended_drag_offer.h"

#include <cstdint>
#include <string>

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/exo/data_offer.h"
#include "components/exo/data_offer_observer.h"
#include "ui/gfx/geometry/vector2d.h"

namespace exo {

ExtendedDragOffer::ExtendedDragOffer(DataOffer* offer, Delegate* delegate)
    : offer_(offer), delegate_(delegate) {
  DCHECK(offer_);
  DCHECK(delegate_);
}

ExtendedDragOffer::~ExtendedDragOffer() {
  delegate_->OnDataOfferDestroying();
}

void ExtendedDragOffer::OnDataOfferDestroying(DataOffer* offer) {
  DCHECK_EQ(offer, offer_);
  offer_ = nullptr;
}

}  // namespace exo
