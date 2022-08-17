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
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace autofill_assistant {

class WebsiteLoginManager;

// An Autofill Assistant client for headless runs.
class ClientHeadless : public Client, public AccessTokenFetcher {
 public:
  explicit ClientHeadless(content::WebContents* web_contents,
                          const CommonDependencies* common_dependencies,
                          ExternalActionDelegate* action_extension_delegate,
                          WebsiteLoginManager* website_login_manager,
                          const base::TickClock* tick_clock,
                          base::WeakPtr<RuntimeManager> runtime_manager,
                          ukm::UkmRecorder* ukm_recorder,
                          AnnotateDomModelService* annotate_dom_model_service);
  ClientHeadless(const ClientHeadless&) = delete;
  ClientHeadless& operator=(const ClientHeadless&) = delete;

  ~ClientHeadless() override;

  bool IsRunning() const;
  void Start(const GURL& url,
             std::unique_ptr<TriggerContext> trigger_context,
             std::unique_ptr<Service> service,
             std::unique_ptr<WebController> web_controller,
             base::OnceCallback<void(Metrics::DropOutReason reason)>
                 script_ended_callback);

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
  void GetAnnotateDomModelVersion(
      base::OnceCallback<void(absl::optional<int64_t>)> callback)
      const override;
  bool GetMakeSearchesAndBrowsingBetterEnabled() const override;
  bool GetMetricsReportingEnabled() const override;

  // Checks if given XML is signed or not.
  bool IsXmlSigned(const std::string& xml_string) const override;
  // Extracts attribute values from the |xml_string| corresponding to the
  // |keys|.
  const std::vector<std::string> ExtractValuesFromSingleTagXml(
      const std::string& xml_string,
      const std::vector<std::string>& keys) const override;

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
  void NotifyScriptEnded(Metrics::DropOutReason reason);

  const raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<Controller> controller_;
  const raw_ptr<const CommonDependencies> common_dependencies_;
  const raw_ptr<WebsiteLoginManager> website_login_manager_;
  std::unique_ptr<HeadlessUiController> headless_ui_controller_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  base::OnceCallback<void(bool, const std::string&)>
      fetch_access_token_callback_;
  const raw_ptr<const base::TickClock> tick_clock_;
  base::WeakPtr<RuntimeManager> runtime_manager_;
  const raw_ptr<ukm::UkmRecorder> ukm_recorder_;
  const raw_ptr<AnnotateDomModelService> annotate_dom_model_service_;

  // Only set while a script is running.
  base::OnceCallback<void(Metrics::DropOutReason reason)>
      script_ended_callback_;

  base::WeakPtrFactory<ClientHeadless> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_CLIENT_HEADLESS_H_
