// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_CLIENT_ANDROID_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_CLIENT_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/android/dependencies_android.h"
#include "components/autofill_assistant/browser/android/ui_controller_android.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace password_manager {
class PasswordChangeSuccessTracker;
}

namespace autofill_assistant {

// Creates a Autofill Assistant client associated with a WebContents.
//
// To obtain an instance of this class from the C++ side, call
// ClientAndroid::FromWebContents(web_contents). To make sure an instance
// exists, call ClientAndroid::CreateForWebContents first.
//
// From the Java side, call AutofillAssistantClient.fromWebContents.
//
// This class is accessible from the Java side through AutofillAssistantClient.
class ClientAndroid : public Client,
                      public AccessTokenFetcher,
                      public content::WebContentsUserData<ClientAndroid> {
 public:
  ClientAndroid(const ClientAndroid&) = delete;
  ClientAndroid& operator=(const ClientAndroid&) = delete;

  ~ClientAndroid() override;

  // Returns the corresponding Java AutofillAssistantClient.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Returns whether a flow is currently running.
  bool IsRunning() const;

  // Returns whether UI is currently being displayed to the user.
  bool IsVisible() const;

  void Start(const GURL& url,
             std::unique_ptr<TriggerContext> trigger_context,
             std::unique_ptr<Service> test_service_to_inject,
             const base::android::JavaRef<jobject>& joverlay_coordinator,
             const absl::optional<TriggerScriptProto>& trigger_script);
  void OnJavaDestroyUI(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& jcaller);

  base::android::ScopedJavaLocalRef<jstring> GetPrimaryAccountName(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  void OnAccessToken(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller,
                     jboolean success,
                     const base::android::JavaParamRef<jstring>& access_token);
  void OnPaymentsClientToken(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jbyteArray>& jclient_token);

  void FetchWebsiteActions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jexperiment_ids,
      const base::android::JavaParamRef<jobjectArray>& jargument_names,
      const base::android::JavaParamRef<jobjectArray>& jargument_values,
      const base::android::JavaParamRef<jobject>& jcallback);

  bool HasRunFirstCheck(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller) const;

  base::android::ScopedJavaLocalRef<jobjectArray> GetDirectActions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

  bool PerformDirectAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jaction_id,
      const base::android::JavaParamRef<jstring>& jexperiment_ids,
      const base::android::JavaParamRef<jobjectArray>& jargument_names,
      const base::android::JavaParamRef<jobjectArray>& jargument_values,
      const base::android::JavaParamRef<jobject>& joverlay_coordinator);

  void ShowFatalError(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& jcaller);

  bool IsSupervisedUser(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jcaller);

  void OnSpokenFeedbackAccessibilityServiceChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean enabled);

  base::android::ScopedJavaGlobalRef<jobject> GetDependencies(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);
  std::string GetDebugContext();

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
  std::string GetLatestCountryCode() const override;
  std::string GetStoredPermanentCountryCode() const override;
  DeviceContext GetDeviceContext() const override;
  security_state::SecurityLevel GetSecurityLevel() const override;
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
  bool IsXmlSigned(const std::string& xml_string) const override;
  const std::vector<std::string> ExtractValuesFromSingleTagXml(
      const std::string& xml_string,
      const std::vector<std::string>& keys) const override;
  bool GetMakeSearchesAndBrowsingBetterEnabled() const override;
  bool GetMetricsReportingEnabled() const override;

  // Overrides AccessTokenFetcher
  void FetchAccessToken(
      base::OnceCallback<void(bool, const std::string&)>) override;
  void InvalidateAccessToken(const std::string& access_token) override;

 private:
  friend class content::WebContentsUserData<ClientAndroid>;

  explicit ClientAndroid(
      content::WebContents* web_contents,
      const base::android::ScopedJavaGlobalRef<jobject>& jdependencies);

  void CreateController(
      std::unique_ptr<Service> service,
      const absl::optional<TriggerScriptProto>& trigger_script);
  void DestroyController();
  void AttachUI(const base::android::JavaRef<jobject>& joverlay_coordinator);
  bool NeedsUI();
  void OnFetchWebsiteActions(const base::android::JavaRef<jobject>& jcallback);
  void SafeDestroyControllerAndUI(Metrics::DropOutReason reason);

  base::android::ScopedJavaLocalRef<jobjectArray>
  GetDirectActionsAsJavaArrayOfStrings(JNIEnv* env) const;

  base::android::ScopedJavaLocalRef<jobject>
  ToJavaAutofillAssistantDirectAction(JNIEnv* env,
                                      const DirectAction& direct_action) const;

  // Returns the index of a direct action with that name, to pass to
  // UiDelegate::PerformUserAction() or -1 if not found.
  int FindDirectAction(const std::string& action_name);

  void OnAnnotateDomModelFileAvailable(
      base::OnceCallback<void(absl::optional<int64_t>)> callback,
      bool available);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Contains AssistantStaticDependencies which do not change.
  const std::unique_ptr<const DependenciesAndroid> dependencies_;
  const raw_ptr<AnnotateDomModelService> annotate_dom_model_service_;
  // Can change based on activity attachment.
  const base::android::ScopedJavaGlobalRef<jobject> jdependencies_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  std::unique_ptr<Controller> controller_;
  std::unique_ptr<UiController> ui_controller_;
  mutable std::unique_ptr<WebsiteLoginManager> website_login_manager_;

  // True if Start() was called. This turns on the tracking of dropouts.
  bool started_ = false;

  // Intent parameter used for tracking dropouts per intent.
  // TODO(b/182164683) Do not store intent paramenter in |ClientAndroid|.
  std::string intent_;

  // True if the UI was ever attached.
  bool has_had_ui_ = false;

  std::unique_ptr<UiControllerAndroid> ui_controller_android_;

  base::OnceCallback<void(bool, const std::string&)>
      fetch_access_token_callback_;
  base::OnceCallback<void(const std::string&)>
      fetch_payments_client_token_callback_;

  base::WeakPtrFactory<ClientAndroid> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_CLIENT_ANDROID_H_
