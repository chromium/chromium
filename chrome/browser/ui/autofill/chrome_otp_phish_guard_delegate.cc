// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_otp_phish_guard_delegate.h"

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/autofill/otp_filling_safe_browsing_checker_client.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/v5_get_hash_protocol_manager_factory.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#endif

namespace autofill {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
namespace {
safe_browsing::PasswordProtectionService* GetProtectionService(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  auto* client = ChromePasswordManagerClient::FromWebContents(web_contents);
  return client ? client->GetPasswordProtectionService() : nullptr;
}
}  // namespace
#endif

ChromeOtpPhishGuardDelegate::ChromeOtpPhishGuardDelegate(
    content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)) {}

ChromeOtpPhishGuardDelegate::~ChromeOtpPhishGuardDelegate() = default;

void ChromeOtpPhishGuardDelegate::StartOtpPhishGuardCheck(
    const GURL& main_frame_url,
    const GURL& frame_to_fill_url,
    base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  auto database_manager =
      g_browser_process->safe_browsing_service()
          ? g_browser_process->safe_browsing_service()->database_manager()
          : nullptr;
  if (safe_browsing::IsSafeBrowsingEnabled(*profile->GetPrefs()) &&
      database_manager) {
    // First, check both the main frame URL and target frame-to-fill URL
    // against local Safe Browsing blocklists covering various threat types.
    // This provides fast, local multi-frame protection for both human and
    // actor workflows.
    auto* v5_get_hash_protocol_manager =
        safe_browsing::V5GetHashProtocolManagerFactory::GetForProfile(profile);
    safe_browsing_checker_client_ =
        OtpFillingSafeBrowsingCheckerClient::CreateAndCheck(
            database_manager,
            v5_get_hash_protocol_manager
                ? v5_get_hash_protocol_manager->GetWeakPtr()
                : nullptr,
            OtpFillingSafeBrowsingCheckerClient::kDefaultCheckDelay,
            main_frame_url, frame_to_fill_url,
            base::BindOnce(
                &ChromeOtpPhishGuardDelegate::OnSafeBrowsingCheckComplete,
                weak_factory_.GetWeakPtr(), main_frame_url,
                std::move(callback)));
    return;
  }
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

  auto* password_manager_client =
      ChromePasswordManagerClient::FromWebContents(&web_contents_.get());
  bool is_actor_task_ongoing =
      password_manager_client && password_manager_client->IsActorTaskActive();

  // If Safe Browsing is disabled/unavailable and an actor task is ongoing,
  // do not consider the check successful (report unsafe / is_malicious = true).
  // Post task to ensure callback is always invoked asynchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), is_actor_task_ongoing));
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
void ChromeOtpPhishGuardDelegate::OnSafeBrowsingCheckComplete(
    const GURL& main_frame_url,
    base::OnceCallback<void(bool)> callback,
    bool is_malicious) {
  safe_browsing_checker_client_.reset();

  auto* pps = GetProtectionService(&web_contents_.get());
  if (is_malicious || !pps) {
    std::move(callback).Run(is_malicious);
    return;
  }

  // If the local safe browsing check passed, perform a PhishGuard reputation
  // request for zero-day phishing detection. This check is performed only on
  // the main frame URL, as server-side PhishGuard models evaluate top-level
  // page identity and visual reputation (subframes are not sent for OTP
  // PhishGuard requests).
  pps->MaybeStartOtpPhishingRequest(&web_contents_.get(), main_frame_url,
                                    std::move(callback));
}
#endif

}  // namespace autofill
