// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_SUBKEY_REQUESTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_SUBKEY_REQUESTER_H_

#include "base/macros.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"

namespace autofill {

// This receives a region code and the device's language.
using SubKeyReceiverCallback =
    base::OnceCallback<void(const std::vector<std::string>&,
                            const std::vector<std::string>&)>;

// SubKeyRequester Loads Rules from the server and extracts the subkeys.
// For a given key (region code for a country, such as US), the list of its
// corresponding subkeys is the list of that countries admin areas (states,
// provinces, ..).
class SubKeyRequester : public LoadRulesListener {
 public:
  // The interface for the subkey request.
  class Request {
   public:
    virtual void OnRulesLoaded() = 0;
    virtual ~Request() {}
  };

  SubKeyRequester(std::unique_ptr<::i18n::addressinput::Source> source,
                  std::unique_ptr<::i18n::addressinput::Storage> storage);
  ~SubKeyRequester() override;

  // If the rules for |region_code| are loaded, this gets the subkeys for the
  // |region_code|,  synchronously. If they are not loaded yet, it sets up a
  // task to get the subkeys when the rules are loaded (asynchronous). If the
  // loading has not yet started, it will also start loading the rules for the
  // |region_code|. The received subkeys will be returned to the |requester|. If
  // the subkeys are not received in |timeout_seconds|, then the requester will
  // be informed and the request will be canceled. |requester| should never be
  // null. The requesting device language is set to |language|, ex:"fr".
  void StartRegionSubKeysRequest(const std::string& region_code,
                                 const std::string& language,
                                 int timeout_seconds,
                                 SubKeyReceiverCallback cb);

  // Returns whether the rules for the specified |region_code| have finished
  // loading.
  bool AreRulesLoadedForRegion(const std::string& region_code);

  // Start loading the rules for the specified |region_code|.
  virtual void LoadRulesForRegion(const std::string& region_code);

  // Cancels the pending subkey request task.
  void CancelPendingGetSubKeys();

 private:
  // Called when the address rules for the |region_code| have finished
  // loading. Implementation of the LoadRulesListener interface.
  void OnAddressValidationRulesLoaded(const std::string& region_code,
                                      bool success) override;

  // The region code and the request for the pending subkey request.
  std::unique_ptr<Request> pending_subkey_request_;
  std::string pending_subkey_region_code_;

  // The address validator used to load subkeys.
  AddressValidator address_validator_;

  DISALLOW_COPY_AND_ASSIGN(SubKeyRequester);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_SUBKEY_REQUESTER_H_
