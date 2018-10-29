// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_H_

#include <string>

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace autofill_assistant {
class UiController;
class AccessTokenFetcher;

// A client interface that needs to be supplied to the controller by the
// embedder.
class Client {
 public:
  virtual ~Client() = default;

  // Returns the API key to be used for requests to the backend.
  virtual std::string GetApiKey() = 0;

  // Returns the AccessTokenFetcher to use to get oauth credentials.
  virtual AccessTokenFetcher* GetAccessTokenFetcher() = 0;

  // Returns the current active personal data manager.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;

  // Returns the server URL to be used for requests to the backend.
  virtual std::string GetServerUrl() = 0;

  // Returns a UiController.
  virtual UiController* GetUiController() = 0;

 protected:
  Client() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_H_
