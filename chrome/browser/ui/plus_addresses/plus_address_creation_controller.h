// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_H_

#include "components/plus_addresses/plus_address_types.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace plus_addresses {
// An interface for orchestrating plus address creation UI.
class PlusAddressCreationController {
 public:
  // Used to allow differing implementations behind a common creation interface.
  // May include some separation on Android vs Desktop, etc., though there is
  // only a single implementation for now. In each case, the controller
  // is owned by the `web_contents` via `WebContentsUserData`.
  static PlusAddressCreationController* GetOrCreate(
      content::WebContents* web_contents);

  // Offers the creation UI, or optionally invokes `callback` immediately.
  // In creation UI scenarios, `callback` may not be run due to user
  // cancellation.
  virtual void OfferCreation(const url::Origin& main_frame_origin,
                             PlusAddressCallback callback) = 0;

  // Sends a request to the server to obtain a plus address after the previous
  // request failed for whatever reason.
  virtual void TryAgainToReservePlusAddress() = 0;

  // Queries the server for a new suggested plus address.
  virtual void OnRefreshClicked() = 0;

  // Run when the creation UI completes with confirmation from the user.
  virtual void OnConfirmed() = 0;
  // Run when plus_address creation is canceled by the user.
  virtual void OnCanceled() = 0;
  // Run when the UI element is destroyed.
  virtual void OnDialogDestroyed() = 0;
};
}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_H_
