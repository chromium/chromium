// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/web_api_handshake_checker.h"

#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"

namespace safe_browsing {

class WebApiHandshakeChecker::CheckerOnSB
    : public base::SupportsWeakPtr<WebApiHandshakeChecker::CheckerOnSB> {
 public:
  CheckerOnSB(base::WeakPtr<WebApiHandshakeChecker> handshake_checker,
              GetDelegateCallback delegate_getter,
              const GetWebContentsCallback& web_contents_getter,
              int frame_tree_node_id)
      : handshake_checker_(std::move(handshake_checker)),
        delegate_getter_(std::move(delegate_getter)),
        web_contents_getter_(web_contents_getter),
        frame_tree_node_id_(frame_tree_node_id) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    DCHECK(handshake_checker_);
    DCHECK(delegate_getter_);
    DCHECK(web_contents_getter_);

    content::WebContents* contents = web_contents_getter_.Run();
    if (!!contents) {
      last_committed_url_ = contents->GetLastCommittedURL();
    }
  }

  void Check(const GURL& url) {
    DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                            ? content::BrowserThread::UI
                            : content::BrowserThread::IO);
    DCHECK(delegate_getter_);
    DCHECK(web_contents_getter_);

    scoped_refptr<UrlCheckerDelegate> url_checker_delegate =
        std::move(delegate_getter_).Run();
    bool skip_checks =
        !url_checker_delegate ||
        url_checker_delegate->ShouldSkipRequestCheck(
            url, frame_tree_node_id_,
            /*render_process_id=*/content::ChildProcessHost::kInvalidUniqueID,
            /*render_frame_id=*/MSG_ROUTING_NONE,
            /*originated_from_service_worker=*/false);
    if (skip_checks) {
      OnCompleteCheck(/*slow_check=*/false, /*proceed=*/true,
                      /*showed_interstitial=*/false,
                      /*did_perform_url_real_time_check=*/false,
                      /*did_check_url_real_time_allowlist=*/false);
      return;
    }

    url_checker_ = std::make_unique<SafeBrowsingUrlCheckerImpl>(
        net::HttpRequestHeaders(), /*load_flags=*/0,
        network::mojom::RequestDestination::kEmpty, /*has_user_gesture=*/false,
        url_checker_delegate, web_contents_getter_,
        /*render_process_id=*/content::ChildProcessHost::kInvalidUniqueID,
        /*render_frame_id=*/MSG_ROUTING_NONE, frame_tree_node_id_,
        /*url_real_time_lookup_enabled=*/false,
        /*can_urt_check_subresource_url=*/false,
        /*can_check_db=*/true, /*can_check_high_confidence_allowlist=*/true,
        /*url_lookup_service_metric_suffix=*/".None", last_committed_url_,
        content::GetUIThreadTaskRunner({}),
        /*url_lookup_service=*/nullptr, WebUIInfoSingleton::GetInstance(),
        /*hash_realtime_service_on_ui=*/nullptr,
        /*mechanism_experimenter=*/nullptr,
        /*is_mechanism_experiment_allowed=*/false,
        /*hash_realtime_selection=*/
        hash_realtime_utils::HashRealTimeSelection::kNone);
    url_checker_->CheckUrl(
        url, "GET",
        base::BindOnce(&WebApiHandshakeChecker::CheckerOnSB::OnCheckUrlResult,
                       base::Unretained(this)));
  }

 private:
  // See comments in BrowserUrlLoaderThrottle::OnCheckUrlResult().
  void OnCheckUrlResult(
      SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier* slow_check_notifier,
      bool proceed,
      bool showed_interstitial,
      bool did_perform_url_real_time_check,
      bool did_check_url_real_time_allowlist) {
    DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                            ? content::BrowserThread::UI
                            : content::BrowserThread::IO);
    if (!slow_check_notifier) {
      OnCompleteCheck(/*slow_check=*/false, proceed, showed_interstitial,
                      did_perform_url_real_time_check,
                      did_check_url_real_time_allowlist);
      return;
    }

    *slow_check_notifier =
        base::BindOnce(&WebApiHandshakeChecker::CheckerOnSB::OnCompleteCheck,
                       base::Unretained(this), /*slow_check=*/true);
  }

  void OnCompleteCheck(bool slow_check,
                       bool proceed,
                       bool showed_interstitial,
                       bool did_perform_url_real_time_check,
                       bool did_check_url_real_time_allowlist) {
    DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                            ? content::BrowserThread::UI
                            : content::BrowserThread::IO);
    if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
      handshake_checker_->OnCompleteCheck(
          slow_check, proceed, showed_interstitial,
          did_perform_url_real_time_check, did_check_url_real_time_allowlist);
    } else {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&WebApiHandshakeChecker::OnCompleteCheck,
                         handshake_checker_, slow_check, proceed,
                         showed_interstitial, did_perform_url_real_time_check,
                         did_check_url_real_time_allowlist));
    }
  }

  base::WeakPtr<WebApiHandshakeChecker> handshake_checker_;
  GetDelegateCallback delegate_getter_;
  GetWebContentsCallback web_contents_getter_;
  const int frame_tree_node_id_;
  GURL last_committed_url_;
  std::unique_ptr<SafeBrowsingUrlCheckerImpl> url_checker_;
};

WebApiHandshakeChecker::WebApiHandshakeChecker(
    GetDelegateCallback delegate_getter,
    const GetWebContentsCallback& web_contents_getter,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  sb_checker_ = std::make_unique<CheckerOnSB>(
      weak_factory_.GetWeakPtr(), std::move(delegate_getter),
      web_contents_getter, frame_tree_node_id);
}

WebApiHandshakeChecker::~WebApiHandshakeChecker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                   std::move(sb_checker_));
  }
}

void WebApiHandshakeChecker::Check(const GURL& url, CheckCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!check_callback_);
  check_callback_ = std::move(callback);
  if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    sb_checker_->Check(url);
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&WebApiHandshakeChecker::CheckerOnSB::Check,
                                  sb_checker_->AsWeakPtr(), url));
  }
}

void WebApiHandshakeChecker::OnCompleteCheck(
    bool slow_check,
    bool proceed,
    bool showed_interstitial,
    bool did_perform_url_real_time_check,
    bool did_check_url_real_time_allowlist) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(check_callback_);

  CheckResult result = proceed ? CheckResult::kProceed : CheckResult::kBlocked;
  std::move(check_callback_).Run(result);
}

}  // namespace safe_browsing
