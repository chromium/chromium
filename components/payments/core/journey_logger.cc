// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/payments/core/journey_logger.h"

#include <algorithm>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/re2/src/re2/re2.h"

namespace payments {

namespace {

// Returns the JourneyLogger histograms name suffix based on the |section| and
// the |completion_status|.
std::string GetHistogramNameSuffix(
    int section,
    JourneyLogger::CompletionStatus completion_status) {
  std::string name_suffix;

  switch (section) {
    case JourneyLogger::SECTION_SHIPPING_ADDRESS:
      name_suffix = "ShippingAddress.";
      break;
    case JourneyLogger::SECTION_CONTACT_INFO:
      name_suffix = "ContactInfo.";
      break;
    case JourneyLogger::SECTION_PAYMENT_METHOD:
      name_suffix = "PaymentMethod.";
      break;
    default:
      break;
  }

  switch (completion_status) {
    case JourneyLogger::COMPLETION_STATUS_COMPLETED:
      name_suffix += "Completed";
      break;
    case JourneyLogger::COMPLETION_STATUS_USER_ABORTED:
      name_suffix += "UserAborted";
      break;
    case JourneyLogger::COMPLETION_STATUS_OTHER_ABORTED:
      name_suffix += "OtherAborted";
      break;
    case JourneyLogger::COMPLETION_STATUS_USER_OPTED_OUT:
      name_suffix += "UserOptedOut";
      break;
    default:
      break;
  }

  DCHECK(!name_suffix.empty());
  return name_suffix;
}

// Returns true when exactly one boolean value in the vector is true.
bool ValidateExclusiveBitVector(const std::vector<bool>& bit_vector) {
  bool seen_true_bit = false;
  for (auto bit : bit_vector) {
    if (!bit)
      continue;
    if (seen_true_bit)
      return false;
    seen_true_bit = true;
  }
  return seen_true_bit;
}

}  // namespace

JourneyLogger::JourneyLogger(ukm::SourceId payment_request_source_id)
    : events2_(static_cast<int>(Event2::kInitiated)),
      payment_request_source_id_(payment_request_source_id) {}

JourneyLogger::~JourneyLogger() {
  // has_recorded_ is false in cases that the page gets closed. To see more
  // details about this case please check sample crash link from
  // dumpWithoutCrash:
  // https://crash.corp.google.com/browse?q=reportid=%27c1268a7104b25de2%27
  UMA_HISTOGRAM_BOOLEAN("PaymentRequest.JourneyLoggerHasRecorded",
                        has_recorded_);
}

void JourneyLogger::SetNumberOfSuggestionsShown(Section section,
                                                int number,
                                                bool has_complete_suggestion) {
  DCHECK_LT(section, SECTION_MAX);
  sections_[section].number_suggestions_shown_ = number;
  sections_[section].is_requested_ = true;
  sections_[section].has_complete_suggestion_ = has_complete_suggestion;
}

void JourneyLogger::SetEvent2Occurred(Event2 event) {
  events2_ |= static_cast<int>(event);
}

void JourneyLogger::SetOptOutOffered() {
  SetEvent2Occurred(Event2::kOptOutOffered);
}

void JourneyLogger::SetActivationlessShow() {
  SetEvent2Occurred(Event2::kActivationlessShow);
}

void JourneyLogger::SetSkippedShow() {
  SetEvent2Occurred(Event2::kSkippedShow);
}

void JourneyLogger::SetShown() {
  SetEvent2Occurred(Event2::kShown);
}

void JourneyLogger::SetPayClicked() {
  SetEvent2Occurred(Event2::kPayClicked);
}

void JourneyLogger::SetSelectedMethod(PaymentMethodCategory category) {
  switch (category) {
    case PaymentMethodCategory::kBasicCard:
      SetEvent2Occurred(Event2::kSelectedCreditCard);
      break;
    case PaymentMethodCategory::kGoogle:
      SetEvent2Occurred(Event2::kSelectedGoogle);
      break;
    case PaymentMethodCategory::kPlayBilling:
      SetEvent2Occurred(Event2::kSelectedPlayBilling);
      break;
    case PaymentMethodCategory::kSecurePaymentConfirmation:
      SetEvent2Occurred(Event2::kSelectedSecurePaymentConfirmation);
      break;
    case PaymentMethodCategory::kGooglePayAuthentication:  // Intentional
                                                           // fallthrough.
    case PaymentMethodCategory::kOther:
      SetEvent2Occurred(Event2::kSelectedOther);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void JourneyLogger::SetRequestedInformation(bool requested_shipping,
                                            bool requested_email,
                                            bool requested_phone,
                                            bool requested_name) {
  // This method should only be called once per Payment Request.
  if (requested_shipping) {
    SetEvent2Occurred(Event2::kRequestShipping);
  }

  if (requested_email) {
    SetEvent2Occurred(Event2::kRequestPayerData);
  }

  if (requested_phone) {
    SetEvent2Occurred(Event2::kRequestPayerData);
  }

  if (requested_name) {
    SetEvent2Occurred(Event2::kRequestPayerData);
  }
}

void JourneyLogger::SetRequestedPaymentMethods(
    const std::vector<PaymentMethodCategory>& methods) {
  for (auto& method : methods) {
    switch (method) {
      case PaymentMethodCategory::kBasicCard:
        SetEvent2Occurred(Event2::kRequestMethodBasicCard);
        break;
      case PaymentMethodCategory::kGoogle:
        SetEvent2Occurred(Event2::kRequestMethodGoogle);
        break;
      case PaymentMethodCategory::kGooglePayAuthentication:
        SetEvent2Occurred(Event2::kRequestMethodGooglePayAuthentication);
        break;
      case PaymentMethodCategory::kPlayBilling:
        SetEvent2Occurred(Event2::kRequestMethodPlayBilling);
        break;
      case PaymentMethodCategory::kSecurePaymentConfirmation:
        SetEvent2Occurred(Event2::kRequestMethodSecurePaymentConfirmation);
        break;
      case PaymentMethodCategory::kOther:
        SetEvent2Occurred(Event2::kRequestMethodOther);
        break;
    }
  }
}

void JourneyLogger::SetCompleted() {
  DCHECK(WasPaymentRequestTriggered());
  RecordJourneyStatsHistograms(COMPLETION_STATUS_COMPLETED);
}

void JourneyLogger::SetAborted(AbortReason reason) {
  if (reason == ABORT_REASON_ABORTED_BY_USER ||
      reason == ABORT_REASON_USER_NAVIGATION)
    RecordJourneyStatsHistograms(COMPLETION_STATUS_USER_ABORTED);
  else if (reason == ABORT_REASON_USER_OPTED_OUT)
    RecordJourneyStatsHistograms(COMPLETION_STATUS_USER_OPTED_OUT);
  else
    RecordJourneyStatsHistograms(COMPLETION_STATUS_OTHER_ABORTED);
}

void JourneyLogger::SetNotShown() {
  DCHECK(!WasPaymentRequestTriggered());
  RecordJourneyStatsHistograms(COMPLETION_STATUS_COULD_NOT_SHOW);
}

void JourneyLogger::SetNoMatchingCredentialsShown() {
  SetShown();
  SetEvent2Occurred(Event2::kNoMatchingCredentials);
}

void JourneyLogger::RecordCheckoutStep(CheckoutFunnelStep step) {
  base::UmaHistogramEnumeration("PaymentRequest.CheckoutFunnel", step);
}

void JourneyLogger::RecordJourneyStatsHistograms(
    CompletionStatus completion_status) {
  if (has_recorded_) {
    UMA_HISTOGRAM_BOOLEAN(
        "PaymentRequest.JourneyLoggerHasRecordedMultipleTimes", true);
  }
  has_recorded_ = true;

  RecordEventsMetric(completion_status);

  // Depending on the completion status record kPaymentRequestTriggered and/or
  // kCompleted checkout steps.
  switch (completion_status) {
    case COMPLETION_STATUS_COMPLETED:
      RecordCheckoutStep(CheckoutFunnelStep::kPaymentRequestTriggered);
      RecordCheckoutStep(CheckoutFunnelStep::kCompleted);
      break;
    case COMPLETION_STATUS_USER_ABORTED:
    case COMPLETION_STATUS_OTHER_ABORTED:
    case COMPLETION_STATUS_USER_OPTED_OUT:
      RecordCheckoutStep(CheckoutFunnelStep::kPaymentRequestTriggered);
      break;
    case COMPLETION_STATUS_COULD_NOT_SHOW:
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  // These following metrics only make sense if the Payment Request was
  // triggered.
  if (WasPaymentRequestTriggered()) {
    RecordSectionSpecificStats(completion_status);
  }
}

void JourneyLogger::RecordSectionSpecificStats(
    CompletionStatus completion_status) {
  for (int i = 0; i < NUMBER_OF_SECTIONS; ++i) {
    std::string name_suffix = GetHistogramNameSuffix(i, completion_status);
    // Only log the metrics for a section if it was requested by the merchant.
    if (sections_[i].is_requested_) {
      base::UmaHistogramCustomCounts(
          "PaymentRequest.NumberOfSuggestionsShown." + name_suffix,
          std::min(sections_[i].number_suggestions_shown_, MAX_EXPECTED_SAMPLE),
          MIN_EXPECTED_SAMPLE, MAX_EXPECTED_SAMPLE, NUMBER_BUCKETS);
    }
  }
}

void JourneyLogger::RecordEventsMetric(CompletionStatus completion_status) {
  // Add the completion status to the events.
  switch (completion_status) {
    case COMPLETION_STATUS_COMPLETED:
      SetEvent2Occurred(Event2::kCompleted);
      break;
    case COMPLETION_STATUS_USER_ABORTED:
      SetEvent2Occurred(Event2::kUserAborted);
      break;
    case COMPLETION_STATUS_OTHER_ABORTED:
      SetEvent2Occurred(Event2::kOtherAborted);
      break;
    case COMPLETION_STATUS_COULD_NOT_SHOW:
      SetEvent2Occurred(Event2::kCouldNotShow);
      break;
    case COMPLETION_STATUS_USER_OPTED_OUT:
      SetEvent2Occurred(Event2::kUserOptedOut);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  // Add whether the user had and initial form of payment to the events.
  if (sections_[SECTION_PAYMENT_METHOD].number_suggestions_shown_ > 0) {
    SetEvent2Occurred(Event2::kHadInitialFormOfPayment);
  }

  // Record the events in UMA.
  ValidateEventBits();
  base::UmaHistogramSparse("PaymentRequest.Events2", events2_);

  if (payment_request_source_id_ == ukm::kInvalidSourceId)
    return;

  // Record the events in UKM.
  ukm::builders::PaymentRequest_CheckoutEvents(payment_request_source_id_)
      .SetCompletionStatus(completion_status)
      .SetEvents2(events2_)
      .Record(ukm::UkmRecorder::Get());

  if (payment_app_source_id_ == ukm::kInvalidSourceId)
    return;

  // Record the events in UKM for payment app.
  ukm::builders::PaymentApp_CheckoutEvents(payment_app_source_id_)
      .SetCompletionStatus(completion_status)
      .SetEvents2(events2_)
      .Record(ukm::UkmRecorder::Get());

  // Clear payment app source id since it gets deleted after recording.
  payment_app_source_id_ = ukm::kInvalidSourceId;
}

bool JourneyLogger::WasOccurred(Event2 event) const {
  return events2_ & static_cast<int>(event);
}

void JourneyLogger::ValidateEventBits() const {
  std::vector<bool> bit_vector;

  // Validate completion status.
  bit_vector.push_back(WasOccurred(Event2::kCompleted));
  bit_vector.push_back(WasOccurred(Event2::kOtherAborted));
  bit_vector.push_back(WasOccurred(Event2::kUserAborted));
  bit_vector.push_back(WasOccurred(Event2::kCouldNotShow));
  bit_vector.push_back(WasOccurred(Event2::kUserOptedOut));
  DCHECK(ValidateExclusiveBitVector(bit_vector));
  bit_vector.clear();

  // Validate the user selected method.
  if (WasOccurred(Event2::kCompleted)) {
    bit_vector.push_back(WasOccurred(Event2::kSelectedCreditCard));
    bit_vector.push_back(WasOccurred(Event2::kSelectedGoogle));
    bit_vector.push_back(
        WasOccurred(Event2::kSelectedSecurePaymentConfirmation));
    bit_vector.push_back(WasOccurred(Event2::kSelectedPlayBilling));
    bit_vector.push_back(WasOccurred(Event2::kSelectedOther));
    DCHECK(ValidateExclusiveBitVector(bit_vector));
    bit_vector.clear();
  }

  // Selected method should be requested.
  if (WasOccurred(Event2::kSelectedCreditCard)) {
    DCHECK(WasOccurred(Event2::kRequestMethodBasicCard));
  } else if (WasOccurred(Event2::kSelectedGoogle)) {
    DCHECK(WasOccurred(Event2::kRequestMethodGoogle));
  } else if (WasOccurred(Event2::kSelectedSecurePaymentConfirmation)) {
    DCHECK(WasOccurred(Event2::kRequestMethodSecurePaymentConfirmation));
  } else if (WasOccurred(Event2::kSelectedOther)) {
    // It is possible that a service worker based app responds to "basic-card"
    // request.
    DCHECK(WasOccurred(Event2::kRequestMethodOther) ||
           WasOccurred(Event2::kRequestMethodBasicCard) ||
           WasOccurred(Event2::kRequestMethodGooglePayAuthentication));
  }

  // Validate UI SHOWN status.
  if (WasOccurred(Event2::kCompleted)) {
    bit_vector.push_back(WasOccurred(Event2::kShown));
    bit_vector.push_back(WasOccurred(Event2::kSkippedShow));
    DCHECK(ValidateExclusiveBitVector(bit_vector));
    bit_vector.clear();
  }

  // Validate skipped UI show.
  if (WasOccurred(Event2::kSkippedShow)) {
    // Built in autofill payment handler for basic card should not skip UI show.
    DCHECK(!WasOccurred(Event2::kSelectedCreditCard));
    // Internal secure payment confirmation payment handler should not skip UI
    // show.
    DCHECK(!WasOccurred(Event2::kSelectedSecurePaymentConfirmation));
  }

  // Validate activationless show.
  if (WasOccurred(Event2::kActivationlessShow)) {
    // Should not be able to record an activationless show without show itself
    // being recorded.
    DCHECK(WasOccurred(Event2::kShown) || WasOccurred(Event2::kSkippedShow));
  }
}

bool JourneyLogger::WasPaymentRequestTriggered() {
  return WasOccurred(Event2::kShown) || WasOccurred(Event2::kSkippedShow);
}

void JourneyLogger::SetPaymentAppUkmSourceId(
    ukm::SourceId payment_app_source_id) {
  payment_app_source_id_ = payment_app_source_id;
}

base::WeakPtr<JourneyLogger> JourneyLogger::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
