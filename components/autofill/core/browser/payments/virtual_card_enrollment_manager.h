// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/strike_databases/payments/virtual_card_enrollment_strike_database.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class WebContents;
}

namespace autofill {

class CreditCard;
class PersonalDataManager;

// This struct is passed into the controller when we show the
// VirtualCardEnrollmentBubble, and it lets the controller customize the
// bubble based on the fields in this struct. For example, we will show
// different last 4 digits of a credit card based on the |credit_card| object
// in this struct.
struct VirtualCardEnrollmentFields {
  VirtualCardEnrollmentFields();
  VirtualCardEnrollmentFields(const VirtualCardEnrollmentFields&);
  VirtualCardEnrollmentFields& operator=(const VirtualCardEnrollmentFields&);
  ~VirtualCardEnrollmentFields();

  bool operator==(const VirtualCardEnrollmentFields& other) const;

  // The credit card to enroll.
  CreditCard credit_card;
  // Raw pointer to the image skia for the card art. If the card art is not yet
  // available, this pointer will be set to the network image after we receive a
  // GetDetailsForEnrollResponse. The |card_art_image| object is owned by
  // PersonalDataManager if it is the card art, or by the resource bundle if it
  // is the network icon.
  raw_ptr<const gfx::ImageSkia> card_art_image = nullptr;
  // The Google-specific legal messages that the user must accept before
  // opting-in to virtual card enrollment.
  LegalMessageLines google_legal_message;
  // The Issuer-specific legal messages that the user must accept before
  // opting-in to virtual card enrollment. Empty for some issuers.
  LegalMessageLines issuer_legal_message;
  // The source for which the VirtualCardEnrollmentBubble will be shown.
  VirtualCardEnrollmentSource virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kNone;
  // A boolean value indicating if this will be the final time the user will see
  // this offer, until strikes eventually expire.  Determined by the number of
  // existing strikes.
  bool last_show = false;
  // A boolean value indicating if such enrollment offer for the card has been
  // declined before.
  bool previously_declined = false;
};

// This struct is used to track the state of the virtual card enrollment
// process, and its members are read from and written to throughout the process
// where needed. It is created and owned by VirtualCardEnrollmentManager.
struct VirtualCardEnrollmentProcessState {
  VirtualCardEnrollmentProcessState();
  VirtualCardEnrollmentProcessState(const VirtualCardEnrollmentProcessState&);
  VirtualCardEnrollmentProcessState& operator=(
      const VirtualCardEnrollmentProcessState&);
  ~VirtualCardEnrollmentProcessState();
  // Only populated once the risk engine responded.
  std::optional<std::string> risk_data;
  // |virtual_card_enrollment_fields|'s |credit_card| and
  // |virtual_card_enrollment_source| are populated in the beginning of the
  // virtual card enrollment flow, but the rest of the fields are only populated
  // before showing the VirtualCardEnrollmentBubble.
  VirtualCardEnrollmentFields virtual_card_enrollment_fields;
  // Populated after the GetDetailsForEnrollResponseDetails are received. Based
  // on the |vcn_context_token| the server is able to retrieve the instrument
  // id, and using |vcn_context_token| for enroll allows the server to link a
  // GetDetailsForEnrollRequest with the corresponding
  // UpdateVirtualCardEnrollmentRequest for the enroll process.
  std::optional<std::string> vcn_context_token;
};

// Owned by PaymentsAutofillClient. There is one instance of this class per tab.
// This class manages the flow for enrolling and unenrolling in Virtual Card
// Numbers.
class VirtualCardEnrollmentManager {
 public:
  // The parameters should outlive the VirtualCardEnrollmentManager.
  VirtualCardEnrollmentManager(
      PersonalDataManager* personal_data_manager,
      payments::PaymentsNetworkInterface* payments_network_interface,
      AutofillClient* autofill_client = nullptr);
  VirtualCardEnrollmentManager(const VirtualCardEnrollmentManager&) = delete;
  VirtualCardEnrollmentManager& operator=(const VirtualCardEnrollmentManager&) =
      delete;
  virtual ~VirtualCardEnrollmentManager();

  using RiskAssessmentFunction = base::OnceCallback<void(
      uint64_t obfuscated_gaia_id,
      PrefService* user_prefs,
      base::OnceCallback<void(const std::string&)> callback,
      content::WebContents* web_contents,
      gfx::Rect window_bounds)>;

  using VirtualCardEnrollmentFieldsLoadedCallback = base::OnceCallback<void(
      VirtualCardEnrollmentFields* virtual_card_enrollment_fields)>;

  using VirtualCardEnrollmentUpdateResponseCallback = base::OnceCallback<void(
      payments::PaymentsAutofillClient::PaymentsRpcResult result)>;

  // Starting point for the VCN enroll flow. The fields in |credit_card| will
  // be used throughout the flow, such as for request fields as well as credit
  // card specific fields for the bubble to display.
  // |virtual_card_enrollment_source| will be used by
  // ShowVirtualCardEnrollBubble() to differentiate different bubbles based on
  // the source we originated from.
  virtual void InitVirtualCardEnroll(
      const CreditCard& credit_card,
      VirtualCardEnrollmentSource virtual_card_enrollment_source,
      // |get_details_for_enrollment_response_details| will be populated if we
      // are in the optimized upstream case, where we receive the
      // GetDetailsForEnrollmentResponseDetails from the
      // UploadCardResponseDetails, so we can then skip the
      // GetDetailsForEnroll request in the Virtual Card Enrollment flow.
      std::optional<payments::PaymentsNetworkInterface::
                        GetDetailsForEnrollmentResponseDetails>
          get_details_for_enrollment_response_details = std::nullopt,
      // |user_prefs| will be populated if we are in the Android settings page,
      // to then be used for loading risk data. Otherwise it will always be
      // nullptr, and we should load risk data through |autofill_client_| as we
      // have access to web contents.
      PrefService* user_prefs = nullptr,
      // Callback that will be run in the Android settings page use cases. It
      // will take in a |callback|, |obfuscated_gaia_id|, and |user_prefs| that
      // will end up being passed into the overloaded risk_util::LoadRiskData()
      // call that does not require web contents.
      RiskAssessmentFunction risk_assessment_function = base::DoNothing(),
      // Callback that be run once the `state_.virtual_card_enrollment_fields_`
      // is loaded from the server response. The callback would trigger the
      // enrollment dialog in the Settings page on Android.
      VirtualCardEnrollmentFieldsLoadedCallback = base::DoNothing());

  // Uses `payments_network_interface_` to send the enroll request. `state_`'s
  // `vcn_context_token_`, which should be set when we receive the
  // GetDetailsForEnrollResponse, is used in the
  // UpdateVirtualCardEnrollmentRequest to enroll the correct card.
  void Enroll(
      // The callback lets the Android Settings page know whether
      // (un)enrollment was successful.
      std::optional<VirtualCardEnrollmentUpdateResponseCallback>
          virtual_card_enrollment_update_response_callback);

  // Unenrolls the card mapped to the given |instrument_id|.
  void Unenroll(int64_t instrument_id,
                std::optional<VirtualCardEnrollmentUpdateResponseCallback>
                    virtual_card_enrollment_update_response_callback);

  // Returns true if a credit card identified by its |instrument_id| should be
  // blocked for virtual card enrollment and is not attempting to enroll from
  // the settings page. Currently we block enrollment offer if the user has
  // reached the limit of strikes or if the required delay time since last
  // strike has not passed yet. Does nothing if the strike database is not
  // available.
  bool ShouldBlockVirtualCardEnrollment(
      const std::string& instrument_id,
      VirtualCardEnrollmentSource virtual_card_enrollment_source) const;

  // Adds a strike to block enrollment for credit card identified by its
  // |instrument_id|. Does nothing if the strike database is not available.
  void AddStrikeToBlockOfferingVirtualCardEnrollment(
      const std::string& instrument_id);

  // Removes potential strikes to block a credit card identified by its
  // |instrument_id| for enrollment. Does nothing if the strike database is not
  // available.
  void RemoveAllStrikesToBlockOfferingVirtualCardEnrollment(
      const std::string& instrument_id);

  // Clears the strikes on the associated virtual card enrollment strike
  // database.
  void ClearAllStrikesForTesting();

  // Sets |save_card_bubble_accepted_timestamp_|, which will be the start time
  // for the LatencySinceUpstream metrics.
  void SetSaveCardBubbleAcceptedTimestamp(
      base::Time save_card_bubble_accepted_timestamp);

 protected:
  // Handles the response from the UpdateVirtualCardEnrollmentRequest. |type|
  // indicates the type of the request sent, i.e., enroll or unenroll.
  // |result| represents the result from the server call to change the virtual
  // card enrollment state for the credit card passed into
  // InitVirtualCardEnroll().
  virtual void OnDidGetUpdateVirtualCardEnrollmentResponse(
      VirtualCardEnrollmentRequestType type,
      payments::PaymentsAutofillClient::PaymentsRpcResult result);

  // Called after virtual card enrollment is completed. Will show enroll result
  // to users.
  void OnVirtualCardEnrollCompleted(
      payments::PaymentsAutofillClient::PaymentsRpcResult result);

  // Resets the state of this VirtualCardEnrollmentManager.
  virtual void Reset();

  // Data in |state_| will be populated with the data we have at the current
  // point of the virtual card enrollment flow we are in. This data will then be
  // used by future points of the flow for actions such as populating request
  // fields, and sending data to the VirtualCardEnrollmentBubbleController to
  // display in the UI. VirtualCardEnrollmentManager::Reset() will reset
  // |state_|.
  VirtualCardEnrollmentProcessState state_;

  // The associated autofill client, used to load risk data and show the
  // VirtualCardEnrollBubble. Weak reference. Can be nullptr, which indicates
  // that we are in the Clank settings page, from which Autofill Client is not
  // accessible.
  raw_ptr<AutofillClient> autofill_client_;

  // Used to get a pointer to the strike database for virtual card enrollment.
  VirtualCardEnrollmentStrikeDatabase* GetVirtualCardEnrollmentStrikeDatabase()
      const;

  // Whether we've received GetDetailsForEnrollResponseDetails.
  bool enroll_response_details_received_ = false;

  // Loads risk data for the respective use case and then continues the virtual
  // card enrollment flow. |user_prefs| will only be present in Clank settings
  // page use cases, as we will not have access to web contents.
  virtual void LoadRiskDataAndContinueFlow(
      PrefService* user_prefs,
      base::OnceCallback<void(const std::string&)> callback);

  // Shows the VirtualCardEnrollmentBubble. |state_|'s
  // |virtual_card_enrollment_fields| will contain all of the dynamic fields
  // VirtualCardEnrollmentBubbleController needs to display the correct bubble.
  virtual void ShowVirtualCardEnrollBubble();

  // Callback triggered after the VirtualCardEnrollmentFields are loaded from
  // the server response. Note: This is only called when the `autofill_client_`
  // is not available.
  VirtualCardEnrollmentFieldsLoadedCallback
      virtual_card_enrollment_fields_loaded_callback_;

  // Callback triggered after getting server response about the success of
  // virtual card (un)enrollment.
  std::optional<VirtualCardEnrollmentUpdateResponseCallback>
      virtual_card_enrollment_update_response_callback_;

  // Cancels the entire Virtual Card enrollment process.
  void OnVirtualCardEnrollmentBubbleCancelled();

 private:
  friend class VirtualCardEnrollmentManagerTest;
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           OnDidGetDetailsForEnrollResponse);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           OnDidGetDetailsForEnrollResponse_Reset);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           OnRiskDataLoadedForVirtualCard);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           OnVirtualCardEnrollmentBubbleAccepted);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           StrikeDatabase_BubbleAccepted);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           StrikeDatabase_BubbleBlocked);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           StrikeDatabase_BubbleCanceled);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           StrikeDatabase_EnrollmentAttemptFailed);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           StrikeDatabase_SettingsPageNotBlocked);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           VirtualCardEnrollmentFields_LastShow);

  // Called once the risk data is loaded. The |risk_data| will be used with
  // |state_|'s |virtual_card_enrollment_fields|'s |credit_card|'s
  // |instrument_id_| field to make a GetDetailsForEnroll request, and
  // |state_|'s |virtual_card_enrollment_source| will be passed down to when we
  // show the bubble so that we show the correct bubble version.
  void OnRiskDataLoadedForVirtualCard(const std::string& risk_data);

  // Sends the GetDetailsForEnrollRequest using `payments_network_interface_`.
  // `state_`'s `risk_data` and its `virtual_card_enrollment_fields`'s
  // `credit_card`'s `instrument_id` are the fields the server requires for the
  // GetDetailsForEnrollRequest, and will be used by
  // `payments_network_interface_`. `state_`'s
  // `virtual_card_enrollment_fields_`'s `virtual_card_enrollment_source` is
  // passed here so that it can be forwarded to ShowVirtualCardEnrollBubble.
  void GetDetailsForEnroll();

  // Handles the response from the GetDetailsForEnrollRequest. |result| and
  // |response| are received from the GetDetailsForEnroll server call response,
  // while |state_| is passed down from GetDetailsForEnroll() to track the
  // current process' state.
  void OnDidGetDetailsForEnrollResponse(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const payments::PaymentsNetworkInterface::
          GetDetailsForEnrollmentResponseDetails& response);

  // Sets the corresponding fields in |state_| from the
  // GetDetailsForEnrollmentResponseDetails in |response|. This function is used
  // both when a GetDetailsForEnrollRequest gets a response, and when offering
  // virtual card enrollment through the optimized upstream flow as the
  // GetDetailsForEnrollmentResponseDetails is returned in the upload card
  // response.
  void SetGetDetailsForEnrollmentResponseDetails(
      const payments::PaymentsNetworkInterface::
          GetDetailsForEnrollmentResponseDetails& response);

  // Should always be called right before showing virtual card enrollment UI.
  // This function attempts to set the card art image in |state_|, and if the
  // card art image is not synced yet from the chrome sync server, it will fall
  // back to the network image.
  void EnsureCardArtImageIsSetBeforeShowingUI();

  // Helper function that is called any time we offer virtual card enroll.
  void SetInitialVirtualCardEnrollFields(
      const CreditCard& credit_card,
      VirtualCardEnrollmentSource virtual_card_enrollment_source);

  // Returns true if the passed in GetDetailsForEnrollmentResponseDetails is
  // valid.
  bool IsValidGetDetailsForEnrollmentResponseDetails(
      const payments::PaymentsNetworkInterface::
          GetDetailsForEnrollmentResponseDetails&
              get_details_for_enrollment_response_details);

  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest, Enroll);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           OnDidGetDetailsForEnrollResponse);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           OnDidGetDetailsForEnrollResponse_NoAutofillClient);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           OnDidGetDetailsForEnrollResponse_Reset);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           OnRiskDataLoadedForVirtualCard);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           UpstreamAnimationSync_AnimationFirst);
  FRIEND_TEST_ALL_PREFIXES(VirtualCardEnrollmentManagerTest,
                           UpstreamAnimationSync_ResponseFirst);

  // The associated personal data manager, used to save and load personal data
  // to/from the web database.
  const raw_ptr<PersonalDataManager> personal_data_manager_;

  // The associated `payments_network_interface_` that is used for all requests
  // to the server.
  const raw_ptr<payments::PaymentsNetworkInterface> payments_network_interface_;

  // The database that is used to count instrument_id-keyed strikes to suppress
  // prompting users to enroll in virtual cards.
  std::unique_ptr<VirtualCardEnrollmentStrikeDatabase>
      virtual_card_enrollment_strike_database_;

  // Used in scenarios where we do not have access to web contents, and need to
  // pass in a callback to the overloaded risk_util::LoadRiskData.
  RiskAssessmentFunction risk_assessment_function_;

  // Used to track the latency metrics between SaveCardBubble accept and
  // VirtualCardEnrollBubble show. Set in ChromeAutofillClient once
  // SaveCardBubble is accepted for upload save, so that we can track the
  // starting timestamp of the latency. Right before showing the
  // VirtualCardEnrollBubble, we will take the difference between the current
  // timestamp and |save_card_bubble_accepted_timestamp_| to log as the latency
  // metric. |save_card_bubble_accepted_timestamp_| will then be reset.
  std::optional<base::Time> save_card_bubble_accepted_timestamp_;

  // The timestamp when a GetDetailsForEnrollment request is sent.
  std::optional<base::Time> get_details_for_enrollment_request_sent_timestamp_;

  base::WeakPtrFactory<VirtualCardEnrollmentManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
