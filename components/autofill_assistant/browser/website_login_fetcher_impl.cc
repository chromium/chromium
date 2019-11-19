// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/website_login_fetcher_impl.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace autofill_assistant {

// Represents a pending form fetcher request which will notify the
// |WebsiteLoginFetcherImpl| when finished.
class WebsiteLoginFetcherImpl::PendingRequest
    : public password_manager::FormFetcher::Consumer {
 public:
  PendingRequest(
      const password_manager::PasswordStore::FormDigest& form_digest,
      const password_manager::PasswordManagerClient* client,
      base::OnceCallback<void(const PendingRequest*)> notify_finished_callback)
      : form_fetcher_(
            std::make_unique<password_manager::FormFetcherImpl>(form_digest,
                                                                client,
                                                                true)),
        notify_finished_callback_(std::move(notify_finished_callback)),
        weak_ptr_factory_(this) {}

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
      base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                     base::BindOnce(&PendingRequest::NotifyFinished,
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
  DISALLOW_COPY_AND_ASSIGN(PendingRequest);
};

// A pending request to fetch all logins that match the specified |form_digest|.
class WebsiteLoginFetcherImpl::PendingFetchLoginsRequest
    : public WebsiteLoginFetcherImpl::PendingRequest {
 public:
  PendingFetchLoginsRequest(
      const password_manager::PasswordStore::FormDigest& form_digest,
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
      logins.emplace_back(match->origin.GetOrigin(),
                          base::UTF16ToUTF8(match->username_value));
    }
    std::move(callback_).Run(logins);
    PendingRequest::OnFetchCompleted();
  }

 private:
  base::OnceCallback<void(std::vector<Login>)> callback_;
};

// A pending request to fetch the password for the specified |login|.
class WebsiteLoginFetcherImpl::PendingFetchPasswordRequest
    : public WebsiteLoginFetcherImpl::PendingRequest {
 public:
  PendingFetchPasswordRequest(
      const password_manager::PasswordStore::FormDigest& form_digest,
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
    std::vector<const autofill::PasswordForm*> matches =
        form_fetcher_->GetNonFederatedMatches();
    for (const auto* match : matches) {
      if (base::UTF16ToUTF8(match->username_value) == login_.username) {
        std::move(callback_).Run(true,
                                 base::UTF16ToUTF8(match->password_value));
        PendingRequest::OnFetchCompleted();
        return;
      }
    }
    std::move(callback_).Run(false, std::string());
    PendingRequest::OnFetchCompleted();
  }

 private:
  Login login_;
  base::OnceCallback<void(bool, std::string)> callback_;
};

WebsiteLoginFetcherImpl::WebsiteLoginFetcherImpl(
    const password_manager::PasswordManagerClient* client)
    : client_(client), weak_ptr_factory_(this) {}

WebsiteLoginFetcherImpl::~WebsiteLoginFetcherImpl() = default;

void WebsiteLoginFetcherImpl::GetLoginsForUrl(
    const GURL& url,
    base::OnceCallback<void(std::vector<Login>)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  password_manager::PasswordStore::FormDigest digest(
      autofill::PasswordForm::Scheme::kHtml, url.GetOrigin().spec(), GURL());
  pending_requests_.emplace_back(std::make_unique<PendingFetchLoginsRequest>(
      digest, client_, std::move(callback),
      base::BindOnce(&WebsiteLoginFetcherImpl::OnRequestFinished,
                     weak_ptr_factory_.GetWeakPtr())));
  pending_requests_.back()->Start();
}

void WebsiteLoginFetcherImpl::GetPasswordForLogin(
    const Login& login,
    base::OnceCallback<void(bool, std::string)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  password_manager::PasswordStore::FormDigest digest(
      autofill::PasswordForm::Scheme::kHtml, login.origin.spec(), GURL());
  pending_requests_.emplace_back(std::make_unique<PendingFetchPasswordRequest>(
      digest, client_, login, std::move(callback),
      base::BindOnce(&WebsiteLoginFetcherImpl::OnRequestFinished,
                     weak_ptr_factory_.GetWeakPtr())));
  pending_requests_.back()->Start();
}

void WebsiteLoginFetcherImpl::OnRequestFinished(const PendingRequest* request) {
  base::EraseIf(pending_requests_, [request](const auto& candidate_request) {
    return candidate_request.get() == request;
  });
}

}  // namespace autofill_assistant
