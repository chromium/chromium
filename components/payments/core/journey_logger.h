// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_JOURNEY_LOGGER_H_
#define COMPONENTS_PAYMENTS_CORE_JOURNEY_LOGGER_H_

#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace payments {

// A class to keep track of different stats during a Payment Request journey. It
// collects different metrics during the course of the checkout flow, like the
// number of credit cards that the user added or edited. The metrics will be
// logged when RecordJourneyStatsHistograms is called with the completion status
// of the Payment Request.
class JourneyLogger {
 public:
  // Note: Java counterparts will be generated for these enums.

  // The different sections of a Payment Request. Used to record journey
  // stats.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: Section
  enum Section {
    SECTION_CONTACT_INFO = 0,
    SECTION_PAYMENT_METHOD = 1,
    SECTION_SHIPPING_ADDRESS = 2,
    SECTION_MAX,
  };

  // Used to log different parameters' effect on whether the transaction was
  // completed.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: CompletionStatus
  enum CompletionStatus {
    COMPLETION_STATUS_COMPLETED = 0,
    COMPLETION_STATUS_USER_ABORTED = 1,
    COMPLETION_STATUS_OTHER_ABORTED = 2,
    COMPLETION_STATUS_COULD_NOT_SHOW = 3,
    COMPLETION_STATUS_MAX,
  };

  // Used to record the different events that happened during the Payment
  // Request.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: Event
  enum Event {
    // Initiated means the PaymentRequest object was constructed.
    EVENT_INITIATED = 0,
    // PaymentRequest was triggered via .show() and a native UI was shown.
    EVENT_SHOWN = 1 << 0,
    EVENT_PAY_CLICKED = 1 << 1,
    EVENT_RECEIVED_INSTRUMENT_DETAILS = 1 << 2,
    // PaymentRequest was triggered via .show() and no UI was shown because we
    // skipped directly to the payment app.
    EVENT_SKIPPED_SHOW = 1 << 3,
    // .complete() was called by the merchant, completing the flow.
    EVENT_COMPLETED = 1 << 4,
    // The user aborted the flow by either dismissing it explicitely, or
    // navigating away (if possible).
    EVENT_USER_ABORTED = 1 << 5,
    // Other reasons for aborting include the merchant calling .abort(), the
    // merchant triggering a navigation, the tab closing, the browser closing,
    // etc. See implementation for details.
    EVENT_OTHER_ABORTED = 1 << 6,
    EVENT_HAD_INITIAL_FORM_OF_PAYMENT = 1 << 7,
    EVENT_HAD_NECESSARY_COMPLETE_SUGGESTIONS = 1 << 8,
    // canMakePayment was called with a result of "true" or "false",
    // respectively. An absence of both events means canMakePayment was not
    // called, or the user was in incognito mode.
    EVENT_CAN_MAKE_PAYMENT_TRUE = 1 << 9,
    EVENT_CAN_MAKE_PAYMENT_FALSE = 1 << 10,
    // Correspond to the merchant specifying requestShipping, requestPayerName,
    // requestPayerEmail, requestPayerPhone.
    EVENT_REQUEST_SHIPPING = 1 << 11,
    EVENT_REQUEST_PAYER_NAME = 1 << 12,
    EVENT_REQUEST_PAYER_EMAIL = 1 << 13,
    EVENT_REQUEST_PAYER_PHONE = 1 << 14,
    // The merchant requested at least one basic-card method.
    EVENT_REQUEST_METHOD_BASIC_CARD = 1 << 15,
    // The merchant requested a Google payment method.
    EVENT_REQUEST_METHOD_GOOGLE = 1 << 16,
    // The merchant requested a non-Google, non-basic-card payment method.
    EVENT_REQUEST_METHOD_OTHER = 1 << 17,
    // The user initiated the transaction using a saved credit card, a Google
    // payment app (e.g., Android Pay), or another payment instrument,
    // respectively.
    EVENT_SELECTED_CREDIT_CARD = 1 << 18,
    EVENT_SELECTED_GOOGLE = 1 << 19,
    EVENT_SELECTED_OTHER = 1 << 20,
    // hasEnrolledInstrument was called with a result of "true" or "false",
    // respectively. An absence of both events means hasEnrolledInstrument was
    // not called, or the user was in incognito mode.
    EVENT_HAS_ENROLLED_INSTRUMENT_TRUE = 1 << 21,
    EVENT_HAS_ENROLLED_INSTRUMENT_FALSE = 1 << 22,
    // True when a NotShownReason is set.
    EVENT_COULD_NOT_SHOW = 1 << 23,
    EVENT_NEEDS_COMPLETION_CONTACT_INFO = 1 << 24,
    EVENT_NEEDS_COMPLETION_PAYMENT = 1 << 25,
    EVENT_NEEDS_COMPLETION_SHIPPING = 1 << 26,
    // Payment apps available (after JIT crawling) at the time show() is called.
    EVENT_AVAILABLE_METHOD_BASIC_CARD = 1 << 27,
    EVENT_AVAILABLE_METHOD_GOOGLE = 1 << 28,
    EVENT_AVAILABLE_METHOD_OTHER = 1 << 29,
    EVENT_ENUM_MAX = 1 << 30,
  };

  // The reason why the Payment Request was aborted.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: AbortReason
  enum AbortReason {
    ABORT_REASON_ABORTED_BY_USER = 0,
    ABORT_REASON_ABORTED_BY_MERCHANT = 1,
    ABORT_REASON_INVALID_DATA_FROM_RENDERER = 2,
    ABORT_REASON_MOJO_CONNECTION_ERROR = 3,
    ABORT_REASON_MOJO_RENDERER_CLOSING = 4,
    ABORT_REASON_INSTRUMENT_DETAILS_ERROR = 5,
    ABORT_REASON_NO_MATCHING_PAYMENT_METHOD = 6,   // Deprecated.
    ABORT_REASON_NO_SUPPORTED_PAYMENT_METHOD = 7,  // Deprecated.
    ABORT_REASON_OTHER = 8,
    ABORT_REASON_USER_NAVIGATION = 9,
    ABORT_REASON_MERCHANT_NAVIGATION = 10,
    ABORT_REASON_MAX,
  };

  // The reason why the Payment Request was not shown to the user.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: NotShownReason
  enum NotShownReason {
    NOT_SHOWN_REASON_NO_MATCHING_PAYMENT_METHOD = 0,
    NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD = 1,
    NOT_SHOWN_REASON_CONCURRENT_REQUESTS = 2,
    NOT_SHOWN_REASON_OTHER = 3,
    NOT_SHOWN_REASON_MAX = 4,
  };

  JourneyLogger(bool is_incognito, ukm::SourceId source_id);
  ~JourneyLogger();

  // Increments the number of selection adds for the specified section.
  void IncrementSelectionAdds(Section section);

  // Increments the number of selection changes for the specified section.
  void IncrementSelectionChanges(Section section);

  // Increments the number of selection edits for the specified section.
  void IncrementSelectionEdits(Section section);

  // Sets the number of suggestions shown for the specified section.
  void SetNumberOfSuggestionsShown(Section section,
                                   int number,
                                   bool has_valid_suggestion);

  // Records the fact that the merchant called CanMakePayment and records its
  // return value.
  void SetCanMakePaymentValue(bool value);

  // Records the fact that the merchant called HasEnrolledInstrument and records
  // its return value.
  void SetHasEnrolledInstrumentValue(bool value);

  // Records that an event occurred.
  void SetEventOccurred(Event event);

  // Records the user information requested by the merchant.
  void SetRequestedInformation(bool requested_shipping,
                               bool requested_email,
                               bool requested_phone,
                               bool requested_name);

  // Records the requested payment method types. A value should be true if at
  // least one payment method in the category (basic-card, google payment method
  // or other url-based payment method, respectively) is requested.
  // TODO(crbug.com/754811): Add support for non-basic-card, non-URL methods.
  void SetRequestedPaymentMethodTypes(bool requested_basic_card,
                                      bool requested_method_google,
                                      bool requested_method_other);

  // Records that the Payment Request was completed successfully, and starts the
  // logging of all the journey metrics.
  void SetCompleted();

  // Records that the Payment Request was aborted along with the reason. Also
  // starts the logging of all the journey metrics.
  void SetAborted(AbortReason reason);

  // Records that the Payment Request was not shown to the user, along with the
  // reason.
  void SetNotShown(NotShownReason reason);

  // Records the transcation amount after converting to USD separated by
  // completion status (complete vs triggered).
  void RecordTransactionAmount(std::string currency,
                               const std::string& value,
                               bool completed);

  // Records when Payment Request .show is called.
  void SetTriggerTime();

 private:
  static const int NUMBER_OF_SECTIONS = 3;

  // Note: These constants should always be in sync with their counterpart in
  // components/payments/content/android/java/src/org/chromium/components/
  // payments/JourneyLogger.java.
  // The minimum expected value of CustomCountHistograms is always set to 1. It
  // is still possible to log the value 0 to that type of histogram.
  const int MIN_EXPECTED_SAMPLE = 1;
  const int MAX_EXPECTED_SAMPLE = 49;
  const int NUMBER_BUCKETS = 50;

  struct SectionStats {
    SectionStats()
        : number_selection_adds_(0),
          number_selection_changes_(0),
          number_selection_edits_(0),
          number_suggestions_shown_(0),
          is_requested_(false),
          has_complete_suggestion_(false) {}

    int number_selection_adds_;
    int number_selection_changes_;
    int number_selection_edits_;
    int number_suggestions_shown_;
    bool is_requested_;
    bool has_complete_suggestion_;
  };

  // Records the histograms for all the sections that were requested by the
  // merchant and for the usage of the CanMakePayment method and its effect on
  // the transaction. This method should be called when the Payment Request has
  // either been completed or aborted.
  void RecordJourneyStatsHistograms(CompletionStatus completion_status);

  // Records the histograms for all the sections that were requested by the
  // merchant.
  void RecordSectionSpecificStats(CompletionStatus completion_status);

  // Records the metric about the different events that happened during the
  // Payment Request.
  void RecordEventsMetric(CompletionStatus completion_status);

  // Records the time between request.show() and request completion/abort.
  void RecordTimeToCheckout(CompletionStatus completion_status) const;

  // Validates the recorded event sequence during the Payment Request.
  void ValidateEventBits() const;

  // Returns whether this Payment Request was triggered (shown or skipped show).
  bool WasPaymentRequestTriggered();

  // Sets needs completion bit in events_ bit field for the given section.
  void SetSectionNeedsCompletion(Section section);

  SectionStats sections_[NUMBER_OF_SECTIONS];
  bool has_recorded_ = false;
  bool is_incognito_;

  // Accumulates the many events that have happened during the Payment Request.
  int events_;

  // Keeps track of whether transaction amounts are recorded or not to catch
  // multiple recording. Triggered is the first index and Completed the second.
  bool has_recorded_transaction_amount_[2] = {false};

  // Stores the time that request.show() is called. This is used to record
  // checkout duration.
  base::TimeTicks trigger_time_;

  ukm::SourceId source_id_;

  DISALLOW_COPY_AND_ASSIGN(JourneyLogger);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_JOURNEY_LOGGER_H_
