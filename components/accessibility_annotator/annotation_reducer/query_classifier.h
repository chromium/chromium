// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_

#include <string>
#include <vector>

namespace annotation_reducer {

enum class AutofillDataType {
  kUnknown,
  // Address
  kAddress,
  kAddressLine1,
  kAddressCity,
  kAddressState,
  kAddressZip,
  kAddressCountry,
  kPhone,
  kEmail,
  kName,
  // Payments
  kIban,
  // Autofill AI entity types
  kVehicle,
  kVehiclePlate,
  kVehicleVin,
  kPassport,
  kDriversLicense,
  kFlightReservation,
  kNationalIdCard,
  kRedressNumber,
  kKnownTravelerNumber,
};

class QueryClassifier {
 public:
  QueryClassifier();
  QueryClassifier(const QueryClassifier&) = delete;
  QueryClassifier& operator=(const QueryClassifier&) = delete;
  ~QueryClassifier();

  AutofillDataType Classify(const std::u16string& query);

 private:
  void InitializeStopWords();
};

}  // namespace annotation_reducer

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_ANNOTATION_REDUCER_QUERY_CLASSIFIER_H_
