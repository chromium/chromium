// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_BASE_DELEGATE_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_BASE_DELEGATE_H_

#include <memory>
#include <string>

class GURL;

namespace autofill {
class AddressNormalizer;
class PersonalDataManager;
class RegionDataLoader;
}  // namespace autofill

class PrefService;

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace payments {

class PaymentRequest;

class PaymentRequestBaseDelegate {
 public:
  virtual ~PaymentRequestBaseDelegate() = default;

  // Gets the PersonalDataManager associated with this PaymentRequest flow.
  // Cannot be null.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;

  virtual const std::string& GetApplicationLocale() const = 0;

  // Returns whether the user is off-the-record.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the URL of the page that is currently being displayed.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Returns a pointer to the address normalizer to use for the duration of this
  // Payment Request.
  virtual autofill::AddressNormalizer* GetAddressNormalizer() = 0;

  // Creates a new region data loader that will self delete, or a test mock.
  virtual autofill::RegionDataLoader* GetRegionDataLoader() = 0;

  // Returns a pointer to the UKM service.
  virtual ukm::UkmRecorder* GetUkmRecorder() = 0;

  // Returns the user's signed-in email address, or empty string if not signed
  // in.
  virtual std::string GetAuthenticatedEmail() const = 0;

  // Gets the pref service for the browser context associated with this
  // PaymentRequest.
  virtual PrefService* GetPrefService() = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_BASE_DELEGATE_H_
