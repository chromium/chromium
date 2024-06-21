// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_JOURNEY_LOGGER_H_
#define COMPONENTS_PAYMENTS_CORE_JOURNEY_LOGGER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
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
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: Section
  enum Section {
    SECTION_CONTACT_INFO = 0,
    SECTION_PAYMENT_METHOD = 1,
    SECTION_SHIPPING_ADDRESS = 2,
    SECTION_MAX,
  };

  // Used to log different parameters' effect on whether the transaction was
  // completed.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: CompletionStatus
  enum CompletionStatus {
    COMPLETION_STATUS_COMPLETED = 0,
    COMPLETION_STATUS_USER_ABORTED = 1,
    COMPLETION_STATUS_OTHER_ABORTED = 2,
    COMPLETION_STATUS_COULD_NOT_SHOW = 3,
    COMPLETION_STATUS_USER_OPTED_OUT = 4,
    COMPLETION_STATUS_MAX,
  };

  // Used to record the different events that happened during the Payment
  // Request.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: Event2
  enum class Event2 {
    // Initiated means the PaymentRequest object was constructed.
    kInitiated = 0,
    // PaymentRequest was triggered via .show() and a native UI was shown.
    kShown = 1 << 0,
    // A payment app was invoked.
    kPayClicked = 1 << 1,
    // Whether any payer data (i.e., name, email, phone)
    // was requested.
    kRequestPayerData = 1 << 2,
    // PaymentRequest was triggered via .show() and no UI was shown because we
    // skipped directly to the payment app.
    kSkippedShow = 1 << 3,
    // .complete() was called by the merchant, completing the flow.
    kCompleted = 1 << 4,
    // The user aborted the flow by either dismissing it explicitly, or
    // navigating away (if possible).
    kUserAborted = 1 << 5,
    // Other reasons for aborting include the merchant calling .abort(), the
    // merchant triggering a navigation, the tab closing, the browser closing,
    // etc. See implementation for details.
    kOtherAborted = 1 << 6,
    // Whether or not any requested method is available.
    kHadInitialFormOfPayment = 1 << 7,
    // An opt-out experience was offered to the user as part of the flow.
    kOptOutOffered = 1 << 8,
    // The user elected to opt-out of the flow (and future flows).
    kUserOptedOut = 1 << 9,

    // .show() was allowed without a user activaiton.
    kActivationlessShow = 1 << 10,

    // Correspond to the merchant specifying requestShipping,
    // requestPayerName,
    // requestPayerEmail, requestPayerPhone.
    kRequestShipping = 1 << 11,

    // The merchent requested a Google Pay Authentication method.
    kRequestMethodGooglePayAuthentication = 1 << 13,
    // The merchant requested a Play Billing payment method.
    kRequestMethodPlayBilling = 1 << 14,
    // The merchant requested at least one basic-card method.
    kRequestMethodBasicCard = 1 << 15,
    // The merchant requested a Google payment method.
    kRequestMethodGoogle = 1 << 16,
    // The merchant requested a non-Google, non-basic-card payment method.
    kRequestMethodOther = 1 << 17,
    // The user initiated the transaction using a saved credit card, a Google
    // payment app (e.g., Android Pay), or another payment instrument,
    // respectively.
    kSelectedCreditCard = 1 << 18,
    kSelectedGoogle = 1 << 19,
    kSelectedOther = 1 << 20,
    kSelectedPlayBilling = 1 << 21,

    // True when a NotShownReason is set.
    kCouldNotShow = 1 << 23,

    // Bits for secure-payment-confirmation method.
    kNoMatchingCredentials = 1 << 29,
    kRequestMethodSecurePaymentConfirmation = 1 << 30,
    kSelectedSecurePaymentConfirmation = 1 << 31,

    kEnumMax = kSelectedSecurePaymentConfirmation,
  };

  // The reason why the Payment Request was aborted.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.payments
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
    ABORT_REASON_USER_OPTED_OUT = 11,
    ABORT_REASON_MAX,
  };

  // The categories of the payment methods.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: PaymentMethodCategory
  enum class PaymentMethodCategory {
    kBasicCard = 0,
    kGoogle = 1,
    kPlayBilling = 2,
    kSecurePaymentConfirmation = 3,
    kOther = 4,
    kGooglePayAuthentication = 5,
    kMaxValue = kGooglePayAuthentication,
  };

  // Records different checkout steps for payment requests. The difference
  // between number of requests recorded for each step and its successor shows
  // the drop-off that happened during that step.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.payments
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: CheckoutFunnelStep
  enum class CheckoutFunnelStep {
    // Payment request has been initiated.
    kInitiated = 0,
    // .show() has been called. (a.k.a. the user has clicked on the checkout/buy
    // button on merchant's site.)
    kShowCalled = 1,
    // Payment request UI has been shown or skipped in favor of the payment
    // handler UI. Drop-off before this step means that the browser could not
    // proceed with the payment request. (e.g. because of no payment app being
    // available for requested payment method(s) or an unsecured origin.)
    kPaymentRequestTriggered = 2,
    // Payment handler UI has been invoked either by skipping to it directly or
    // the user clicking on the "Continue" button in payment sheet UI.
    kPaymentHandlerInvoked = 3,
    // Payment request has been completed with 'success' status.
    kCompleted = 4,
    kMaxValue = kCompleted,
  };

  explicit JourneyLogger(ukm::SourceId payment_request_source_id);

  JourneyLogger(const JourneyLogger&) = delete;
  JourneyLogger& operator=(const JourneyLogger&) = delete;

  ~JourneyLogger();

  // Sets the number of suggestions shown for the specified section.
  void SetNumberOfSuggestionsShown(Section section,
                                   int number,
                                   bool has_complete_suggestion);

  // Records that an Opt Out experience is being offered to the user in the
  // current UI flow.
  void SetOptOutOffered();

  // Records that a show() was allowed without a user activation.
  void SetActivationlessShow();

  // Records that a payment app has been shown without payment UIs being shown
  // before that.
  void SetSkippedShow();

  // Records that a payment UI has been shown.
  void SetShown();

  // Records that a payment app was invoked.
  void SetPayClicked();

  // Records the category of the selected app.
  void SetSelectedMethod(PaymentMethodCategory category);

  // Records the user information requested by the merchant.
  void SetRequestedInformation(bool requested_shipping,
                               bool requested_email,
                               bool requested_phone,
                               bool requested_name);

  // Records the requested payment method types. A value should be true if at
  // least one payment method in the category (basic-card, google payment
  // method, secure payment confirmation method or other url-based payment
  // method, respectively) is requested.
  // TODO(crbug.com/40534824): Add support for non-basic-card, non-URL methods.
  void SetRequestedPaymentMethods(
      const std::vector<PaymentMethodCategory>& methods);

  // Records that the Payment Request was completed successfully, and starts the
  // logging of all the journey metrics.
  void SetCompleted();

  // Records that the Payment Request was aborted. This counts as a completion,
  // starting the logging of all the journey metrics.
  void SetAborted(AbortReason reason);

  // Records that the Payment Request was not shown to the user.
  void SetNotShown();

  // Records that the SPC No Matching Credentials UX was shown to the user.
  void SetNoMatchingCredentialsShown();

  // Increments the bucket count for the given checkout step.
  void RecordCheckoutStep(CheckoutFunnelStep step);

  // Sets the UKM source id of the selected app when it gets invoked.
  void SetPaymentAppUkmSourceId(ukm::SourceId payment_app_source_id);

  base::WeakPtr<JourneyLogger> GetWeakPtr();

 private:
  // Records that an event occurred.
  void SetEvent2Occurred(Event2 event);

  // Whether the given event was occurred.
  bool WasOccurred(Event2 event) const;

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
        : number_suggestions_shown_(0),
          is_requested_(false),
          has_complete_suggestion_(false) {}

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

  // Validates the recorded event sequence during the Payment Request.
  void ValidateEventBits() const;

  // Returns whether this Payment Request was triggered (shown or skipped show).
  bool WasPaymentRequestTriggered();

  SectionStats sections_[NUMBER_OF_SECTIONS];
  bool has_recorded_ = false;

  // Accumulates the many events that have happened during the Payment Request.
  int events2_;

  ukm::SourceId payment_request_source_id_;
  ukm::SourceId payment_app_source_id_ = ukm::kInvalidSourceId;

  base::WeakPtrFactory<JourneyLogger> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_JOURNEY_LOGGER_H_
