// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_COLLECT_USER_DATA_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_COLLECT_USER_DATA_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/website_login_fetcher.h"

namespace autofill_assistant {

// Shows a UI to collect user data required for subsequent actions.
class CollectUserDataAction : public Action,
                              public autofill::PersonalDataManagerObserver {
 public:
  explicit CollectUserDataAction(ActionDelegate* delegate,
                                 const ActionProto& proto);
  ~CollectUserDataAction() override;

  // From autofill::PersonalDataManagerObserver.
  void OnPersonalDataChanged() override;

  static bool IsUserDataComplete(
      const UserData& user_data,
      const CollectUserDataOptions& collect_user_data_options);

 private:
  struct LoginDetails {
    LoginDetails(bool choose_automatically_if_no_other_options,
                 const std::string& payload);
    LoginDetails(bool choose_automatically_if_no_other_options,
                 const std::string& payload,
                 const WebsiteLoginFetcher::Login& login);
    ~LoginDetails();
    bool choose_automatically_if_no_other_options;
    std::string payload;
    // Only for Chrome PWM login details.
    base::Optional<WebsiteLoginFetcher::Login> login;
  };

  void InternalProcessAction(ProcessActionCallback callback) override;
  void EndAction(const ClientStatus& status);

  void OnGetUserData(const CollectUserDataProto& collect_user_data,
                     std::unique_ptr<UserData> user_data);
  void OnAdditionalActionTriggered(int index);
  void OnTermsAndConditionsLinkClicked(int link);

  void OnGetLogins(
      const LoginDetailsProto::LoginOptionProto& login_option,
      std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
      std::vector<WebsiteLoginFetcher::Login> logins);
  void ShowToUser(
      std::unique_ptr<CollectUserDataOptions> collect_user_data_options);

  // Creates a new instance of |CollectUserDataOptions| from |proto_|.
  std::unique_ptr<CollectUserDataOptions> CreateOptionsFromProto();

  // Will update |initial_card_has_billing_postal_code_|.
  bool CheckInitialAutofillDataComplete(
      autofill::PersonalDataManager* personal_data_manager,
      const CollectUserDataOptions& collect_user_data_options);

  // Update user data with the new state from personal data manager.
  void UpdatePersonalDataManagerFields(
      const CollectUserDataOptions* collect_user_data_options,
      UserData* user_data,
      UserData::FieldChange* field_change = nullptr);

  bool shown_to_user_ = false;
  bool initially_prefilled = false;
  bool personal_data_changed_ = false;
  bool action_successful_ = false;
  bool initial_card_has_billing_postal_code_ = false;
  ProcessActionCallback callback_;

  // Maps login choice identifiers to the corresponding login details.
  std::map<std::string, std::unique_ptr<LoginDetails>> login_details_map_;

  base::WeakPtrFactory<CollectUserDataAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CollectUserDataAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_COLLECT_USER_DATA_ACTION_H_
