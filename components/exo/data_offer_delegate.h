// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_DATA_OFFER_DELEGATE_H_
#define COMPONENTS_EXO_DATA_OFFER_DELEGATE_H_

#include <string>

namespace exo {

class DataOffer;
class SecurityDelegate;
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

  // Returns the server's SecurityDelegate.
  virtual SecurityDelegate* GetSecurityDelegate() const = 0;

 protected:
  virtual ~DataOfferDelegate() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_DATA_OFFER_DELEGATE_H_
