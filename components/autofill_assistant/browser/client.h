// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_H_

#include <string>

#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/metrics.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace autofill_assistant {
class AccessTokenFetcher;
class WebsiteLoginFetcher;

// A client interface that needs to be supplied to the controller by the
// embedder.
class Client {
 public:
  virtual ~Client() = default;

  // Attaches the controller to the UI.
  //
  // This method does whatever is necessary to guarantee that, at the end of the
  // call, there is a Controller associated with a UI.
  virtual void AttachUI() = 0;

  // Destroys the UI immediately.
  virtual void DestroyUI() = 0;

  // Returns the API key to be used for requests to the backend.
  virtual std::string GetApiKey() = 0;

  // Returns the e-mail address that corresponds to the auth credentials. Might
  // be empty.
  virtual std::string GetAccountEmailAddress() = 0;

  // Returns the AccessTokenFetcher to use to get oauth credentials.
  virtual AccessTokenFetcher* GetAccessTokenFetcher() = 0;

  // Returns the current active personal data manager.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() = 0;

  // Returns the currently active login fetcher.
  virtual WebsiteLoginFetcher* GetWebsiteLoginFetcher() = 0;

  // Returns the server URL to be used for requests to the backend.
  virtual std::string GetServerUrl() = 0;

  // Returns the locale.
  virtual std::string GetLocale() = 0;

  // Returns the country code.
  virtual std::string GetCountryCode() = 0;

  // Returns details about the device.
  virtual DeviceContext GetDeviceContext() = 0;

  // Stops autofill assistant for the current WebContents, both controller and
  // UI.
  virtual void Shutdown(Metrics::DropOutReason reason) = 0;

 protected:
  Client() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_H_
