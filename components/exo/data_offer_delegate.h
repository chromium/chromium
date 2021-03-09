// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_OFFER_DELEGATE_H_
#define COMPONENTS_EXO_DATA_OFFER_DELEGATE_H_

#include <string>

namespace exo {

class DataOffer;
enum class DndAction;

// Handles events on data devices in context-specific ways.
class DataOfferDelegate {
 public:
  // Called at the top of the data device's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnDataOfferDestroying(DataOffer* offer) = 0;

  // Called when |mime_type| is offered by the client.
  virtual void OnOffer(const std::string& mime_type) = 0;

  // Called when possible |source_actions| is offered by the client.
  virtual void OnSourceActions(
      const base::flat_set<DndAction>& source_actions) = 0;

  // Called when current |action| is offered by the client.
  virtual void OnAction(DndAction action) = 0;

 protected:
  virtual ~DataOfferDelegate() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_OFFER_DELEGATE_H_
