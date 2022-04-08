// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/website_login_manager_impl.h"

#include "base/containers/cxx20_erase.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/form_saver.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

namespace {

// Creates a |PasswordForm| with minimal initialization (origin, username,
// password).
password_manager::PasswordForm CreatePasswordForm(
    const WebsiteLoginManager::Login& login,
    const std::string& password) {
  password_manager::PasswordForm form;
  form.url = login.origin.DeprecatedGetOriginAsURL();
  form.signon_realm = password_manager::GetSignonRealm(form.url);
  form.username_value = base::UTF8ToUTF16(login.username);
  form.password_value = base::UTF8ToUTF16(password);

  return form;
}

// Returns the first match that has same username.
const password_manager::PasswordForm* FindPasswordForLogin(
    const std::vector<const password_manager::PasswordForm*>& matches,
    const WebsiteLoginManager::Login& login) {
  auto result =
      std::find_if(matches.begin(), matches.end(), [&](const auto& match) {
        return base::UTF16ToUTF8(match->username_value) == login.username;
      });
  if (result == matches.end()) {
    return nullptr;
  }
  return *result;
}

}  // namespace

// Represents a pending form fetcher request which will notify the
// |WebsiteLoginManagerImpl| when finished.
class WebsiteLoginManagerImpl::PendingRequest
    : public password_manager::FormFetcher::Consumer {
 public:
  PendingRequest(
      const password_manager::PasswordFormDigest& form_digest,
      const password_manager::PasswordManagerClient* client,
      base::OnceCallback<void(const PendingRequest*)> notify_finished_callback)
      : form_fetcher_(
            std::make_unique<password_manager::FormFetcherImpl>(form_digest,
                                                                client,
                                                                true)),
        notify_finished_callback_(std::move(notify_finished_callback)),
        weak_ptr_factory_(this) {}

  PendingRequest(const PendingRequest&) = delete;
  PendingRequest& operator=(const PendingRequest&) = delete;

  ~PendingRequest() override = default;
  void Start() {
    // Note: Currently, |FormFetcherImpl| has the default state NOT_WAITING.
    // This has the unfortunate side effect that new consumers are immediately
    // notified with an empty result. As a workaround to avoid this first
    // notification, we register as consumer *after* we call |Fetch|.
    form_fetcher_->Fetch();
    form_fetcher_->AddConsumer(this);
  }

 protected:
  // From password_manager::FormFetcher::Consumer:
  // This implementation should be called by subclasses when they are finished.
  void OnFetchCompleted() override {
    // This needs to be done asynchronously, because it will lead to the
    // destruction of |this|, which needs to happen *after* this call has
    // returned.
    if (notify_finished_callback_) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&PendingRequest::NotifyFinished,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(notify_finished_callback_)));
    }
  }
  std::unique_ptr<password_manager::FormFetcher> form_fetcher_;

 private:
  void NotifyFinished(
      base::OnceCallback<void(const PendingRequest*)> callback) {
    form_fetcher_->RemoveConsumer(this);
    std::move(callback).Run(this);
  }

  base::OnceCallback<void(const PendingRequest*)> notify_finished_callback_;
  base::WeakPtrFactory<PendingRequest> weak_ptr_factory_;
};

// A pending request to fetch all logins that match the specified |form_digest|.
class WebsiteLoginManagerImpl::PendingFetchLoginsRequest
    : public WebsiteLoginManagerImpl::PendingRequest {
 public:
  PendingFetchLoginsRequest(
      const password_manager::PasswordFormDigest& form_digest,
      const password_manager::PasswordManagerClient* client,
      base::OnceCallback<void(std::vector<Login>)> callback,
      base::OnceCallback<void(const PendingRequest*)> notify_finished_callback)
      : PendingRequest(form_digest,
                       client,
                       std::move(notify_finished_callback)),
        callback_(std::move(callback)) {}

 protected:
  // From PendingRequest:
  void OnFetchCompleted() override {
    std::vector<Login> logins;
    for (const auto* match : form_fetcher_->GetBestMatches()) {
      logins.emplace_back(match->url.DeprecatedGetOriginAsURL(),
                          base::UTF16ToUTF8(match->username_value));
    }
    std::move(callback_).Run(logins);
    PendingRequest::OnFetchCompleted();
  }

 private:
  base::OnceCallback<void(std::vector<Login>)> callback_;
};

// A pending request to fetch the password for the specified |login|.
class WebsiteLoginManagerImpl::PendingFetchPasswordRequest
    : public WebsiteLoginManagerImpl::PendingRequest {
 public:
  PendingFetchPasswordRequest(
      const password_manager::PasswordFormDigest& form_digest,
      const password_manager::PasswordManagerClient* client,
      const Login& login,
      base::OnceCallback<void(bool, std::string)> callback,
      base::OnceCallback<void(const PendingRequest*)> notify_finished_callback)
      : PendingRequest(form_digest,
                       client,
                       std::move(notify_finished_callback)),
        login_(login),
        callback_(std::move(callback)) {}

 protected:
  // From PendingRequest:
  void OnFetchCompleted() override {
    std::vector<const password_manager::PasswordForm*> matches =
        form_fetcher_->GetNonFederatedMatches();
    const auto* match = FindPasswordForLogin(matches, login_);
    if (match != nullptr) {
      std::move(callback_).Run(true, base::UTF16ToUTF8(match->password_value));
      PendingRequest::OnFetchCompleted();
      return;
    }
    std::move(callback_).Run(false, std::string());
    PendingRequest::OnFetchCompleted();
  }

 private:
  Login login_;
  base::OnceCallback<void(bool, std::string)> callback_;
};

// A pending request to fetch the last time a password was used for the
// specified |login|.
class WebsiteLoginManagerImpl::PendingFetchLastTimePasswordUseRequest
    : public WebsiteLoginManagerImpl::PendingRequest {
 public:
  PendingFetchLastTimePasswordUseRequest(
      const password_manager::PasswordFormDigest& form_digest,
      const password_manager::PasswordManagerClient* client,
      const Login& login,
      base::OnceCallback<void(absl::optional<base::Time>)> callback,
      base::OnceCallback<void(const PendingRequest*)> notify_finished_callback)
      : PendingRequest(form_digest,
                       client,
                       std::move(notify_finished_callback)),
        login_(login),
        callback_(std::move(callback)) {}

 protected:
  // From PendingRequest:
  void OnFetchCompleted() override {
    std::vector<const password_manager::PasswordForm*> matches =
        form_fetcher_->GetNonFederatedMatches();
    const auto* match = FindPasswordForLogin(matches, login_);

    if (!match) {
      std::move(callback_).Run(absl::nullopt);
      PendingRequest::OnFetchCompleted();
      return;
    }

    std::move(callback_).Run(match->date_last_used);
    PendingRequest::OnFetchCompleted();
  }

 private:
  Login login_;
  base::OnceCallback<void(absl::optional<base::Time>)> callback_;
};

// A pending request to delete the password for the specified |login|.
class WebsiteLoginManagerImpl::PendingDeletePasswordRequest
    : public WebsiteLoginManagerImpl::PendingRequest {
 public:
  PendingDeletePasswordRequest(
      const password_manager::PasswordFormDigest& form_digest,
      const password_manager::PasswordManagerClient* client,
      const Login& login,
      base::OnceCallback<void(bool)> callback,
      base::OnceCallback<void(const PendingRequest*)> notify_finished_callback)
      : PendingRequest(form_digest,
                       client,
                       std::move(notify_finished_callback)),
        login_(login),
        callback_(std::move(callback)),
        store_(client->GetProfilePasswordStore()) {}

 protected:
  // From PendingRequest:
  void OnFetchCompleted() override {
    std::vector<const password_manager::PasswordForm*> matches =
        form_fetcher_->GetNonFederatedMatches();
    const auto* match = FindPasswordForLogin(matches, login_);
    if (match != nullptr) {
      store_->RemoveLogin(*match);
    }

    std::move(callback_).Run(match != nullptr);
    PendingRequest::OnFetchCompleted();
  }

 private:
  Login login_;
  base::OnceCallback<void(bool)> callback_;
  raw_ptr<password_manager::PasswordStoreInterface> store_;
};

// A pending request to edit the password for the specified |login|.
class WebsiteLoginManagerImpl::PendingEditPasswordRequest
    : public WebsiteLoginManagerImpl::PendingRequest {
 public:
  PendingEditPasswordRequest(
      const password_manager::PasswordFormDigest& form_digest,
      const password_manager::PasswordManagerClient* client,
      const Login& login,
      const std::string& new_password,
      base::OnceCallback<void(bool)> callback,
      base::OnceCallback<void(const PendingRequest*)> notify_finished_callback)
      : PendingRequest(form_digest,
                       client,
                       std::move(notify_finished_callback)),
        login_(login),
        new_password_(new_password),
        callback_(std::move(callback)),
        form_saver_(std::make_unique<password_manager::FormSaverImpl>(
            client->GetProfilePasswordStore())) {}

 protected:
  // From PendingRequest:
  void OnFetchCompleted() override {
    std::vector<const password_manager::PasswordForm*> matches =
        form_fetcher_->GetNonFederatedMatches();
    const auto* old_password_form = FindPasswordForLogin(matches, login_);
    if (old_password_form != nullptr) {
      password_manager::PasswordForm new_password_form = *old_password_form;
      new_password_form.password_value = base::UTF8ToUTF16(new_password_);
      form_saver_->Update(new_password_form, matches,
                          /*old_password=*/old_password_form->password_value);
    }
    std::move(callback_).Run(old_password_form != nullptr);
    PendingRequest::OnFetchCompleted();
  }

 private:
  Login login_;
  std::string new_password_;
  base::OnceCallback<void(bool)> callback_;
  // Using form saver instead store itself gives us benefit of updating
  // password for all matches (i.e. m.domain.com and www.domain.com' passwords
  // will stay synced)
  std::unique_ptr<password_manager::FormSaver> form_saver_;
};

// A request to update store with new password for a login.
class WebsiteLoginManagerImpl::UpdatePasswordRequest
    : public password_manager::FormFetcher::Consumer {
 public:
  UpdatePasswordRequest(const Login& login,
                        const std::string& password,
                        const autofill::FormData& form_data,
                        password_manager::PasswordManagerClient* client,
                        base::OnceCallback<void()> presaving_completed_callback)
      : password_form_(CreatePasswordForm(login, password)),
        form_data_(form_data),
        client_(client),
        presaving_completed_callback_(std::move(presaving_completed_callback)),
        password_save_manager_(
            std::make_unique<password_manager::PasswordSaveManagerImpl>(
                client)),
        metrics_recorder_(
            base::MakeRefCounted<password_manager::PasswordFormMetricsRecorder>(
                client->IsCommittedMainFrameSecure(),
                client->GetUkmSourceId(),
                client->GetPrefs())),
        votes_uploader_(client, true /* is_possible_change_password_form */) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    password_manager::PasswordFormDigest digest(
        password_manager::PasswordForm::Scheme::kHtml,
        password_form_.signon_realm, password_form_.url);
    form_fetcher_ = std::make_unique<password_manager::FormFetcherImpl>(
        digest, client, true);
  }

  // Password will be presaved when |form_fetcher_| completes fetching.
  void FetchAndPresave() {
    form_fetcher_->Fetch();
    form_fetcher_->AddConsumer(this);
  }

  void SaveGeneratedPassword() {
    password_save_manager_->Save(&form_data_ /* observed_form */,
                                 password_form_);
  }

  // password_manager::FormFetcher::Consumer
  void OnFetchCompleted() override {
    password_save_manager_->Init(client_, form_fetcher_.get(),
                                 metrics_recorder_, &votes_uploader_);
    password_save_manager_->PresaveGeneratedPassword(password_form_);
    password_save_manager_->CreatePendingCredentials(
        password_form_, &form_data_ /* observed_form */,
        form_data_ /* submitted_form */, false /* is_http_auth */,
        false /* is_credential_api_save */);

    if (presaving_completed_callback_) {
      std::move(presaving_completed_callback_).Run();
    }
  }

 private:
  const password_manager::PasswordForm password_form_;
  const autofill::FormData form_data_;
  const raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;
  // This callback will execute when presaving is completed.
  base::OnceCallback<void()> presaving_completed_callback_;
  const std::unique_ptr<password_manager::PasswordSaveManager>
      password_save_manager_;
  scoped_refptr<password_manager::PasswordFormMetricsRecorder>
      metrics_recorder_;
  password_manager::VotesUploader votes_uploader_;
  std::unique_ptr<password_manager::FormFetcher> form_fetcher_;
};

WebsiteLoginManagerImpl::WebsiteLoginManagerImpl(
    password_manager::PasswordManagerClient* client,
    content::WebContents* web_contents)
    : client_(client),
      web_contents_(web_contents),
      leak_delegate_(
          std::make_unique<SavePasswordLeakDetectionDelegate>(client_)),
      weak_ptr_factory_(this) {}

WebsiteLoginManagerImpl::~WebsiteLoginManagerImpl() = default;

void WebsiteLoginManagerImpl::GetLoginsForUrl(
    const GURL& url,
    base::OnceCallback<void(std::vector<Login>)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  password_manager::PasswordFormDigest digest(
      password_manager::PasswordForm::Scheme::kHtml,
      url.DeprecatedGetOriginAsURL().spec(), GURL());
  pending_requests_.emplace_back(std::make_unique<PendingFetchLoginsRequest>(
      digest, client_, std::move(callback),
      base::BindOnce(&WebsiteLoginManagerImpl::OnRequestFinished,
                     weak_ptr_factory_.GetWeakPtr())));
  pending_requests_.back()->Start();
}

void WebsiteLoginManagerImpl::GetPasswordForLogin(
    const Login& login,
    base::OnceCallback<void(bool, std::string)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  password_manager::PasswordFormDigest digest(
      password_manager::PasswordForm::Scheme::kHtml, login.origin.spec(),
      GURL());
  pending_requests_.emplace_back(std::make_unique<PendingFetchPasswordRequest>(
      digest, client_, login, std::move(callback),
      base::BindOnce(&WebsiteLoginManagerImpl::OnRequestFinished,
                     weak_ptr_factory_.GetWeakPtr())));
  pending_requests_.back()->Start();
}

void WebsiteLoginManagerImpl::DeletePasswordForLogin(
    const Login& login,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  password_manager::PasswordFormDigest digest(
      password_manager::PasswordForm::Scheme::kHtml, login.origin.spec(),
      GURL());
  pending_requests_.push_back(std::make_unique<PendingDeletePasswordRequest>(
      digest, client_, login, std::move(callback),
      base::BindOnce(&WebsiteLoginManagerImpl::OnRequestFinished,
                     weak_ptr_factory_.GetWeakPtr())));
  pending_requests_.back()->Start();
}

void WebsiteLoginManagerImpl::GetGetLastTimePasswordUsed(
    const Login& login,
    base::OnceCallback<void(absl::optional<base::Time>)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  password_manager::PasswordFormDigest digest(
      password_manager::PasswordForm::Scheme::kHtml, login.origin.spec(),
      GURL());
  pending_requests_.emplace_back(
      std::make_unique<PendingFetchLastTimePasswordUseRequest>(
          digest, client_, login, std::move(callback),
          base::BindOnce(&WebsiteLoginManagerImpl::OnRequestFinished,
                         weak_ptr_factory_.GetWeakPtr())));
  pending_requests_.back()->Start();
}

void WebsiteLoginManagerImpl::EditPasswordForLogin(
    const Login& login,
    const std::string& new_password,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  password_manager::PasswordFormDigest digest(
      password_manager::PasswordForm::Scheme::kHtml, login.origin.spec(),
      GURL());
  pending_requests_.push_back(std::make_unique<PendingEditPasswordRequest>(
      digest, client_, login, new_password, std::move(callback),
      base::BindOnce(&WebsiteLoginManagerImpl::OnRequestFinished,
                     weak_ptr_factory_.GetWeakPtr())));
  pending_requests_.back()->Start();
}
absl::optional<std::string> WebsiteLoginManagerImpl::GeneratePassword(
    autofill::FormSignature form_signature,
    autofill::FieldSignature field_signature,
    uint64_t max_length) {
  auto* factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          web_contents_);
  DCHECK(factory);
  // TODO(crbug.com/1043132): Add support for non-main frames. If another
  // frame has a different origin than the main frame, passwords-related
  // features may not work.
  auto* driver = factory->GetDriverForFrame(web_contents_->GetMainFrame());
  if (!driver) {
    return absl::nullopt;
  }

  return base::UTF16ToUTF8(
      driver->GetPasswordGenerationHelper()->GeneratePassword(
          driver->GetLastCommittedURL(), form_signature, field_signature,
          max_length));
}

void WebsiteLoginManagerImpl::PresaveGeneratedPassword(
    const Login& login,
    const std::string& password,
    const autofill::FormData& form_data,
    base::OnceCallback<void()> callback) {
  DCHECK(!update_password_request_);
  update_password_request_ = std::make_unique<UpdatePasswordRequest>(
      login, password, form_data, client_, std::move(callback));

  update_password_request_->FetchAndPresave();
}

bool WebsiteLoginManagerImpl::ReadyToSaveGeneratedPassword() {
  return update_password_request_ != nullptr;
}

void WebsiteLoginManagerImpl::SaveGeneratedPassword() {
  DCHECK(update_password_request_);

  update_password_request_->SaveGeneratedPassword();

  update_password_request_.reset();
}

void WebsiteLoginManagerImpl::ResetPendingCredentials() {
  client_->GetPasswordManager()->ResetPendingCredentials();
}

bool WebsiteLoginManagerImpl::ReadyToSaveSubmittedPassword() {
  return client_->GetPasswordManager()->HasSubmittedManager();
}

bool WebsiteLoginManagerImpl::SubmittedPasswordIsSame() {
  return client_->GetPasswordManager()->HasSubmittedManagerWithSamePassword();
}

void WebsiteLoginManagerImpl::CheckWhetherSubmittedCredentialIsLeaked(
    SavePasswordLeakDetectionDelegate::Callback callback,
    base::TimeDelta timeout) {
  absl::optional<password_manager::PasswordForm> credentials =
      client_->GetPasswordManager()->GetSubmittedCredentials();

  if (!credentials.has_value()) {
    std::move(callback).Run(
        LeakDetectionStatus(LeakDetectionStatusCode::NO_USERNAME), false);
    return;
  }

  leak_delegate_->StartLeakCheck(credentials.value(), std::move(callback),
                                 timeout);
}

bool WebsiteLoginManagerImpl::SaveSubmittedPassword() {
  if (!ReadyToSaveSubmittedPassword()) {
    return false;
  }
  client_->GetPasswordManager()->SaveSubmittedManager();
  return true;
}

void WebsiteLoginManagerImpl::OnRequestFinished(const PendingRequest* request) {
  base::EraseIf(pending_requests_, [request](const auto& candidate_request) {
    return candidate_request.get() == request;
  });
}

}  // namespace autofill_assistant
