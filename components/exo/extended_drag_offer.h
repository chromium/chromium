// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_EXTENDED_DRAG_OFFER_H_
#define COMPONENTS_EXO_EXTENDED_DRAG_OFFER_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "components/exo/data_offer_observer.h"

namespace exo {

class DataOffer;

class ExtendedDragOffer : public DataOfferObserver {
 public:
  class Delegate {
   public:
    virtual void OnDataOfferDestroying() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  ExtendedDragOffer(DataOffer* offer, Delegate* delegate);
  ExtendedDragOffer(const ExtendedDragOffer&) = delete;
  ExtendedDragOffer& operator=(const ExtendedDragOffer&) = delete;
  ~ExtendedDragOffer() override;

 private:
  // DataOfferObserver:
  void OnDataOfferDestroying(DataOffer* offer) override;

  raw_ptr<DataOffer> offer_ = nullptr;

  // Created and destroyed at wayland/zcr_extended_drag.cc and its lifetime is
  // tied to the zcr_extended_drag_source_v1 object it's attached to.
  const raw_ptr<Delegate> delegate_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_EXTENDED_DRAG_OFFER_H_
