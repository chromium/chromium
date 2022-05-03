// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_CLIENT_HEADLESS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_CLIENT_HEADLESS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/common_dependencies.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/headless/headless_ui_controller.h"
#include "components/autofill_assistant/browser/platform_dependencies.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace autofill_assistant {

// An Autofill Assistant client for headless runs.
class ClientHeadless : public Client, public AccessTokenFetcher {
 public:
  explicit ClientHeadless(content::WebContents* web_contents,
                          const CommonDependencies* common_dependencies,
                          ExternalActionDelegate* action_extension_delegate);
  ClientHeadless(const ClientHeadless&) = delete;
  ClientHeadless& operator=(const ClientHeadless&) = delete;

  ~ClientHeadless() override;

  bool IsRunning() const;
  void Start(const GURL& url,
             std::unique_ptr<TriggerContext> trigger_context,
             ControllerObserver* observer);

  // Overrides Client
  void AttachUI() override;
  void DestroyUISoon() override;
  void DestroyUI() override;
  version_info::Channel GetChannel() const override;
  std::string GetEmailAddressForAccessTokenAccount() const override;
  std::string GetSignedInEmail() const override;
  absl::optional<std::pair<int, int>> GetWindowSize() const override;
  ClientContextProto::ScreenOrientation GetScreenOrientation() const override;
  void FetchPaymentsClientToken(
      base::OnceCallback<void(const std::string&)> callback) override;
  AccessTokenFetcher* GetAccessTokenFetcher() override;
  autofill::PersonalDataManager* GetPersonalDataManager() const override;
  WebsiteLoginManager* GetWebsiteLoginManager() const override;
  password_manager::PasswordChangeSuccessTracker*
  GetPasswordChangeSuccessTracker() const override;
  std::string GetLocale() const override;
  std::string GetCountryCode() const override;
  DeviceContext GetDeviceContext() const override;
  bool IsAccessibilityEnabled() const override;
  bool IsSpokenFeedbackAccessibilityServiceEnabled() const override;
  content::WebContents* GetWebContents() const override;
  void Shutdown(Metrics::DropOutReason reason) override;
  void RecordDropOut(Metrics::DropOutReason reason) override;
  bool HasHadUI() const override;
  ScriptExecutorUiDelegate* GetScriptExecutorUiDelegate() override;
  bool MustUseBackendData() const override;

  // Overrides AccessTokenFetcher
  void FetchAccessToken(
      base::OnceCallback<void(bool, const std::string&)>) override;
  void InvalidateAccessToken(const std::string& access_token) override;

 private:
  void CreateController();
  void DestroyController();
  void SafeDestroyController(Metrics::DropOutReason reason);
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);

  content::WebContents* web_contents_;
  std::unique_ptr<Controller> controller_;
  const raw_ptr<const CommonDependencies> common_dependencies_;
  std::unique_ptr<WebsiteLoginManager> website_login_manager_;
  std::unique_ptr<HeadlessUiController> headless_ui_controller_;
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  base::OnceCallback<void(bool, const std::string&)>
      fetch_access_token_callback_;

  base::WeakPtrFactory<ClientHeadless> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_CLIENT_HEADLESS_H_
