// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

JourneyLogger::JourneyLogger(bool is_incognito,
                             ukm::SourceId payment_request_source_id)
    : is_incognito_(is_incognito),
      events_(EVENT_INITIATED),
      events2_(static_cast<int>(Event2::kInitiated)),
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

void JourneyLogger::SetSectionNeedsCompletion(const Section section) {
  switch (section) {
    case SECTION_CONTACT_INFO:
      events_ |= EVENT_NEEDS_COMPLETION_CONTACT_INFO;
      break;
    case SECTION_PAYMENT_METHOD:
      events_ |= EVENT_NEEDS_COMPLETION_PAYMENT;
      break;
    case SECTION_SHIPPING_ADDRESS:
      events_ |= EVENT_NEEDS_COMPLETION_SHIPPING;
      break;
    default:
      NOTREACHED();
  }
}

void JourneyLogger::SetCanMakePaymentValue(bool value) {
  // Do not log the outcome of canMakePayment in incognito mode.
  if (is_incognito_)
    return;

  SetEventOccurred(value ? EVENT_CAN_MAKE_PAYMENT_TRUE
                         : EVENT_CAN_MAKE_PAYMENT_FALSE);
}

void JourneyLogger::SetHasEnrolledInstrumentValue(bool value) {
  if (is_incognito_)
    return;

  SetEventOccurred(value ? EVENT_HAS_ENROLLED_INSTRUMENT_TRUE
                         : EVENT_HAS_ENROLLED_INSTRUMENT_FALSE);
}

void JourneyLogger::SetEventOccurred(Event event) {
  events_ |= event;
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
  SetEventOccurred(EVENT_SKIPPED_SHOW);
  SetEvent2Occurred(Event2::kSkippedShow);
}

void JourneyLogger::SetShown() {
  SetEventOccurred(EVENT_SHOWN);
  SetEvent2Occurred(Event2::kShown);
}

void JourneyLogger::SetReceivedInstrumentDetails() {
  SetEventOccurred(EVENT_RECEIVED_INSTRUMENT_DETAILS);
}

void JourneyLogger::SetPayClicked() {
  SetEventOccurred(EVENT_PAY_CLICKED);
  SetEvent2Occurred(Event2::kPayClicked);
}

void JourneyLogger::SetSelectedMethod(PaymentMethodCategory category) {
  switch (category) {
    case PaymentMethodCategory::kBasicCard:
      SetEventOccurred(EVENT_SELECTED_CREDIT_CARD);
      SetEvent2Occurred(Event2::kSelectedCreditCard);
      break;
    case PaymentMethodCategory::kGoogle:
      SetEventOccurred(EVENT_SELECTED_GOOGLE);
      SetEvent2Occurred(Event2::kSelectedGoogle);
      break;
    case PaymentMethodCategory::kPlayBilling:
      SetEvent2Occurred(Event2::kSelectedPlayBilling);
      break;
    case PaymentMethodCategory::kSecurePaymentConfirmation:
      SetEventOccurred(EVENT_SELECTED_SECURE_PAYMENT_CONFIRMATION);
      SetEvent2Occurred(Event2::kSelectedSecurePaymentConfirmation);
      break;
    case PaymentMethodCategory::kGooglePayAuthentication:  // Intentional
                                                           // fallthrough.
    case PaymentMethodCategory::kOther:
      SetEventOccurred(EVENT_SELECTED_OTHER);
      SetEvent2Occurred(Event2::kSelectedOther);
      break;
    default:
      NOTREACHED();
  }
}

void JourneyLogger::SetAvailableMethod(PaymentMethodCategory category) {
  switch (category) {
    case PaymentMethodCategory::kBasicCard:
      SetEventOccurred(EVENT_AVAILABLE_METHOD_BASIC_CARD);
      break;
    case PaymentMethodCategory::kGoogle:
      SetEventOccurred(EVENT_AVAILABLE_METHOD_GOOGLE);
      break;
    case PaymentMethodCategory::kGooglePayAuthentication:  // Intentional
                                                           // fallthrough.
    case PaymentMethodCategory::kPlayBilling:  // Intentional fallthrough.
    case PaymentMethodCategory::kSecurePaymentConfirmation:
      NOTREACHED();
      break;
    case PaymentMethodCategory::kOther:
      SetEventOccurred(EVENT_AVAILABLE_METHOD_OTHER);
      break;
    default:
      NOTREACHED();
  }
}

void JourneyLogger::SetRequestedInformation(bool requested_shipping,
                                            bool requested_email,
                                            bool requested_phone,
                                            bool requested_name) {
  // This method should only be called once per Payment Request.
  if (requested_shipping) {
    SetEventOccurred(EVENT_REQUEST_SHIPPING);
    SetEvent2Occurred(Event2::kRequestShipping);
  }

  if (requested_email) {
    SetEventOccurred(EVENT_REQUEST_PAYER_EMAIL);
    SetEvent2Occurred(Event2::kRequestPayerData);
  }

  if (requested_phone) {
    SetEventOccurred(EVENT_REQUEST_PAYER_PHONE);
    SetEvent2Occurred(Event2::kRequestPayerData);
  }

  if (requested_name) {
    SetEventOccurred(EVENT_REQUEST_PAYER_NAME);
    SetEvent2Occurred(Event2::kRequestPayerData);
  }
}

void JourneyLogger::SetRequestedPaymentMethods(
    const std::vector<PaymentMethodCategory>& methods) {
  for (auto& method : methods) {
    switch (method) {
      case PaymentMethodCategory::kBasicCard:
        SetEventOccurred(EVENT_REQUEST_METHOD_BASIC_CARD);
        SetEvent2Occurred(Event2::kRequestMethodBasicCard);
        break;
      case PaymentMethodCategory::kGoogle:
        SetEventOccurred(EVENT_REQUEST_METHOD_GOOGLE);
        SetEvent2Occurred(Event2::kRequestMethodGoogle);
        break;
      case PaymentMethodCategory::kGooglePayAuthentication:
        SetEvent2Occurred(Event2::kRequestMethodGooglePayAuthentication);
        break;
      case PaymentMethodCategory::kPlayBilling:
        SetEvent2Occurred(Event2::kRequestMethodPlayBilling);
        break;
      case PaymentMethodCategory::kSecurePaymentConfirmation:
        SetEventOccurred(EVENT_REQUEST_METHOD_SECURE_PAYMENT_CONFIRMATION);
        SetEvent2Occurred(Event2::kRequestMethodSecurePaymentConfirmation);
        break;
      case PaymentMethodCategory::kOther:
        SetEventOccurred(EVENT_REQUEST_METHOD_OTHER);
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
  // Always record the first abort reason regardless of whether the
  // PaymentRequest.show() was triggered or not.
  base::UmaHistogramEnumeration("PaymentRequest.CheckoutFunnel.Aborted", reason,
                                ABORT_REASON_MAX);

  if (reason == ABORT_REASON_ABORTED_BY_USER ||
      reason == ABORT_REASON_USER_NAVIGATION)
    RecordJourneyStatsHistograms(COMPLETION_STATUS_USER_ABORTED);
  else if (reason == ABORT_REASON_USER_OPTED_OUT)
    RecordJourneyStatsHistograms(COMPLETION_STATUS_USER_OPTED_OUT);
  else
    RecordJourneyStatsHistograms(COMPLETION_STATUS_OTHER_ABORTED);
}

void JourneyLogger::SetNotShown(NotShownReason reason) {
  DCHECK(!WasPaymentRequestTriggered());
  RecordJourneyStatsHistograms(COMPLETION_STATUS_COULD_NOT_SHOW);
  base::UmaHistogramEnumeration("PaymentRequest.CheckoutFunnel.NoShow", reason,
                                NOT_SHOWN_REASON_MAX);
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
      NOTREACHED();
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
      SetEventOccurred(EVENT_COMPLETED);
      SetEvent2Occurred(Event2::kCompleted);
      break;
    case COMPLETION_STATUS_USER_ABORTED:
      SetEventOccurred(EVENT_USER_ABORTED);
      SetEvent2Occurred(Event2::kUserAborted);
      break;
    case COMPLETION_STATUS_OTHER_ABORTED:
      SetEventOccurred(EVENT_OTHER_ABORTED);
      SetEvent2Occurred(Event2::kOtherAborted);
      break;
    case COMPLETION_STATUS_COULD_NOT_SHOW:
      SetEventOccurred(EVENT_COULD_NOT_SHOW);
      SetEvent2Occurred(Event2::kCouldNotShow);
      break;
    case COMPLETION_STATUS_USER_OPTED_OUT:
      SetEvent2Occurred(Event2::kUserOptedOut);
      break;
    default:
      NOTREACHED();
  }

  // Add the whether the user had complete suggestions for all requested
  // sections to the events.
  bool user_had_complete_suggestions_for_requested_information = true;
  bool is_showing_suggestions = false;
  for (int i = 0; i < NUMBER_OF_SECTIONS; ++i) {
    if (sections_[i].is_requested_) {
      is_showing_suggestions = true;
      if (sections_[i].number_suggestions_shown_ == 0 ||
          !sections_[i].has_complete_suggestion_) {
        user_had_complete_suggestions_for_requested_information = false;
        SetSectionNeedsCompletion(static_cast<const Section>(i));
      }
    }
  }
  if (is_showing_suggestions &&
      user_had_complete_suggestions_for_requested_information) {
    events_ |= EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS;
  }

  // Add whether the user had and initial form of payment to the events.
  if (sections_[SECTION_PAYMENT_METHOD].number_suggestions_shown_ > 0) {
    SetEventOccurred(EVENT_HAD_INITIAL_FORM_OF_PAYMENT);
    SetEvent2Occurred(Event2::kHadInitialFormOfPayment);
  }

  // Record the events in UMA.
  ValidateEventBits();
  base::UmaHistogramSparse("PaymentRequest.Events", events_);
  base::UmaHistogramSparse("PaymentRequest.Events2", events2_);

  if (payment_request_source_id_ == ukm::kInvalidSourceId)
    return;

  // Record the events in UKM.
  ukm::builders::PaymentRequest_CheckoutEvents(payment_request_source_id_)
      .SetCompletionStatus(completion_status)
      .SetEvents(events_)
      .SetEvents2(events2_)
      .Record(ukm::UkmRecorder::Get());

  if (payment_app_source_id_ == ukm::kInvalidSourceId)
    return;

  // Record the events in UKM for payment app.
  ukm::builders::PaymentApp_CheckoutEvents(payment_app_source_id_)
      .SetCompletionStatus(completion_status)
      .SetEvents(events_)
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
  if (events_ & EVENT_COMPLETED)
    DCHECK(events_ & EVENT_PAY_CLICKED);

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
  if (events_ & EVENT_SELECTED_CREDIT_CARD) {
    DCHECK(events_ & EVENT_REQUEST_METHOD_BASIC_CARD);
  } else if (events_ & EVENT_SELECTED_GOOGLE) {
    DCHECK(events_ & EVENT_REQUEST_METHOD_GOOGLE);
  } else if (events_ & EVENT_SELECTED_SECURE_PAYMENT_CONFIRMATION) {
    DCHECK(events_ & EVENT_REQUEST_METHOD_SECURE_PAYMENT_CONFIRMATION);
  } else if (events_ & EVENT_SELECTED_OTHER) {
    // It is possible that a service worker based app responds to "basic-card"
    // request.
    DCHECK(events_ & EVENT_REQUEST_METHOD_OTHER ||
           events_ & EVENT_REQUEST_METHOD_BASIC_CARD ||
           WasOccurred(Event2::kRequestMethodGooglePayAuthentication));
  }

  // Validate UI SHOWN status.
  if (events_ & EVENT_COMPLETED) {
    bit_vector.push_back(events_ & EVENT_SHOWN);
    bit_vector.push_back(events_ & EVENT_SKIPPED_SHOW);
    DCHECK(ValidateExclusiveBitVector(bit_vector));
    bit_vector.clear();
  }

  // Validate skipped UI show.
  if (events_ & EVENT_SKIPPED_SHOW) {
    // Built in autofill payment handler for basic card should not skip UI show.
    DCHECK(!(events_ & EVENT_SELECTED_CREDIT_CARD));
    // Internal secure payment confirmation payment handler should not skip UI
    // show.
    DCHECK(!(events_ & EVENT_SELECTED_SECURE_PAYMENT_CONFIRMATION));
  }

  // Validate activationless show.
  if (WasOccurred(Event2::kActivationlessShow)) {
    // Should not be able to record an activationless show without show itself
    // being recorded.
    DCHECK(WasOccurred(Event2::kShown) || WasOccurred(Event2::kSkippedShow));
  }

  // Check that the two bits are not set at the same time.
  DCHECK(!(events_ & EVENT_CAN_MAKE_PAYMENT_TRUE) ||
         !(events_ & EVENT_CAN_MAKE_PAYMENT_FALSE));

  AssertOccurredTogether(EVENT_SHOWN, Event2::kShown);
  AssertOccurredTogether(EVENT_SKIPPED_SHOW, Event2::kSkippedShow);
  AssertOccurredTogether(EVENT_COULD_NOT_SHOW, Event2::kCouldNotShow);
  AssertOccurredTogether(EVENT_PAY_CLICKED, Event2::kPayClicked);
  AssertOccurredTogether(EVENT_COMPLETED, Event2::kCompleted);
  AssertOccurredTogether(EVENT_USER_ABORTED, Event2::kUserAborted);
  AssertOccurredTogether(EVENT_OTHER_ABORTED, Event2::kOtherAborted);
  AssertOccurredTogether(EVENT_HAD_INITIAL_FORM_OF_PAYMENT,
                         Event2::kHadInitialFormOfPayment);
  AssertOccurredTogether(EVENT_REQUEST_SHIPPING, Event2::kRequestShipping);
  AssertOccurredTogether(EVENT_REQUEST_METHOD_BASIC_CARD,
                         Event2::kRequestMethodBasicCard);
  AssertOccurredTogether(EVENT_REQUEST_METHOD_GOOGLE,
                         Event2::kRequestMethodGoogle);
  AssertOccurredTogether(EVENT_REQUEST_METHOD_OTHER,
                         Event2::kRequestMethodOther);
  AssertOccurredTogether(EVENT_REQUEST_METHOD_SECURE_PAYMENT_CONFIRMATION,
                         Event2::kRequestMethodSecurePaymentConfirmation);
  AssertOccurredTogether(EVENT_SELECTED_CREDIT_CARD,
                         Event2::kSelectedCreditCard);
  AssertOccurredTogether(EVENT_SELECTED_GOOGLE, Event2::kSelectedGoogle);
  AssertOccurredTogether(EVENT_SELECTED_OTHER, Event2::kSelectedOther);
  AssertOccurredTogether(EVENT_SELECTED_SECURE_PAYMENT_CONFIRMATION,
                         Event2::kSelectedSecurePaymentConfirmation);
}

void JourneyLogger::AssertOccurredTogether(Event event, Event2 event2) const {
  DCHECK(event == static_cast<int>(event2));
  DCHECK((events_ & event) == (events2_ & static_cast<int>(event2)));
}

bool JourneyLogger::WasPaymentRequestTriggered() {
  return (events_ & EVENT_SHOWN) > 0 || (events_ & EVENT_SKIPPED_SHOW) > 0;
}

void JourneyLogger::SetPaymentAppUkmSourceId(
    ukm::SourceId payment_app_source_id) {
  payment_app_source_id_ = payment_app_source_id;
}

base::WeakPtr<JourneyLogger> JourneyLogger::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
