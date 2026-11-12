// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_handler_window_android.h"

#include <jni.h>

#include <optional>
#include <utility>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/kill.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/PaymentRequestService_jni.h"

namespace payments::android {
namespace {

using ShowWindowCallback = base::OnceCallback<void(bool /* success */,
                                                   int /* render_process_id */,
                                                   int /* render_frame_id */)>;

// Observes the Payment Handler WebContents until its main frame finishes its
// initial navigation (or fails / is destroyed), invokes `callback`
// asynchronously on the UI thread, and self-deletes. This mirrors
// service_worker_client_utils' OpenURLObserver, which the fallback path
// (ServiceTabLauncher) uses: insecure pages are not rejected here, because
// PaymentHandlerMediator closes the UI and aborts the payment for them.
class FirstNavigationObserver : public content::WebContentsObserver {
 public:
  FirstNavigationObserver(content::WebContents* web_contents,
                          ShowWindowCallback callback)
      : content::WebContentsObserver(web_contents),
        frame_tree_node_id_(
            web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId()),
        callback_(std::move(callback)) {}

  FirstNavigationObserver(const FirstNavigationObserver&) = delete;
  FirstNavigationObserver& operator=(const FirstNavigationObserver&) = delete;

  ~FirstNavigationObserver() override = default;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetFrameTreeNodeId() != frame_tree_node_id_) {
      // Not the main frame that this observer is interested in.
      return;
    }

    if (!navigation_handle->HasCommitted()) {
      RunCallback(false, 0, 0);
      return;
    }

    content::RenderFrameHost* render_frame_host =
        navigation_handle->GetRenderFrameHost();
    RunCallback(true, render_frame_host->GetProcess()->GetDeprecatedID(),
                render_frame_host->GetRoutingID());
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    RunCallback(false, 0, 0);
  }

  void WebContentsDestroyed() override { RunCallback(false, 0, 0); }

 private:
  void RunCallback(bool success, int render_process_id, int render_frame_id) {
    CHECK(web_contents());
    CHECK(callback_);

    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::SequencedTaskRunner::GetCurrentDefault();
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(callback_), success,
                                         render_process_id, render_frame_id));
    Observe(nullptr);
    task_runner->DeleteSoon(FROM_HERE, this);
  }

  const content::FrameTreeNodeId frame_tree_node_id_;
  ShowWindowCallback callback_;
};

}  // namespace

void ShowPaymentHandlerWindow(const GURL& url,
                              std::optional<int> preferred_height,
                              ShowWindowCallback callback) {
  // TODO(crbug.com/582305328): Decide whether to match these checks:
  // - Skipped vs the fallback path (ServiceWorkerVersion::OpenWindow()):
  //   CanRequestURL(), guest-only process, running worker, Activate().
  // - Unmatched vs desktop, which looks up the payment flow per BrowserContext:
  //   no check that the service worker and the showing PaymentRequest share a
  //   BrowserContext.
  content::WebContents* payment_handler_web_contents =
      Java_PaymentRequestService_openPaymentHandlerWindowWithPreferredHeight(
          base::android::AttachCurrentThread(), url, preferred_height);
  if (!payment_handler_web_contents) {
    std::move(callback).Run(false, 0, 0);
    return;
  }

  new FirstNavigationObserver(payment_handler_web_contents,
                              std::move(callback));
}

}  // namespace payments::android

DEFINE_JNI(PaymentRequestService)
