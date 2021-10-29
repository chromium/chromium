// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_navigation_throttle.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/pdf/browser/pdf_stream_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "pdf/pdf_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace pdf {

namespace {

// Used to scope the posted navigation task to the lifetime of `web_contents_`.
//
// Could use `WebContents::FromFrameTreeNodeId()` instead, but this doesn't work
// with a `MockNavigationHandle`.
class WebContentsLifetimeHelper
    : public content::WebContentsUserData<WebContentsLifetimeHelper> {
 public:
  base::WeakPtr<WebContentsLifetimeHelper> GetWeakPtr() const {
    return weak_factory_.GetWeakPtr();
  }

  void OpenUrl(const content::OpenURLParams& params) {
    // `MimeHandlerViewGuest` navigates its embedder for calls to
    // `WebContents::OpenURL()`, so use `LoadURLWithParams()` directly instead.
    web_contents_->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(params));

    // Note that we don't need to register the stream's URL loader as a
    // subresource, as `MimeHandlerViewGuest::ReadyToCommitNavigation()` will
    // handle this as soon as we navigate to a non-`kPdfExtensionId` URL.
  }

 private:
  friend class content::WebContentsUserData<WebContentsLifetimeHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit WebContentsLifetimeHelper(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  content::WebContents* web_contents_;
  base::WeakPtrFactory<WebContentsLifetimeHelper> weak_factory_{this};
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsLifetimeHelper);

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
PdfNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<PdfStreamDelegate> stream_delegate) {
  if (!base::FeatureList::IsEnabled(chrome_pdf::features::kPdfUnseasoned))
    return nullptr;

  if (navigation_handle->IsInMainFrame())
    return nullptr;

  return std::make_unique<PdfNavigationThrottle>(navigation_handle,
                                                 std::move(stream_delegate));
}

PdfNavigationThrottle::PdfNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<PdfStreamDelegate> stream_delegate)
    : content::NavigationThrottle(navigation_handle),
      stream_delegate_(std::move(stream_delegate)) {
  DCHECK(stream_delegate_);
}

PdfNavigationThrottle::~PdfNavigationThrottle() = default;

const char* PdfNavigationThrottle::GetNameForLogging() {
  return "PdfNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
PdfNavigationThrottle::WillStartRequest() {
  // Ignore unless navigating to the stream URL.
  content::WebContents* contents = navigation_handle()->GetWebContents();
  if (!contents)
    return PROCEED;

  const absl::optional<GURL> original_url = stream_delegate_->MapToOriginalUrl(
      contents, navigation_handle()->GetURL());
  if (!original_url.has_value())
    return PROCEED;

  // Uses the same pattern as `PDFIFrameNavigationThrottle` to redirect
  // navigation to the original URL. We'll use this to navigate to the correct
  // origin, while `PdfURLLoaderRequestInterceptor` will intercept the request
  // and replace its content.
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.url = original_url.value();
  params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  params.is_renderer_initiated = false;
  params.is_pdf = true;

  WebContentsLifetimeHelper::CreateForWebContents(contents);
  WebContentsLifetimeHelper* helper =
      WebContentsLifetimeHelper::FromWebContents(contents);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&WebContentsLifetimeHelper::OpenUrl,
                                helper->GetWeakPtr(), std::move(params)));
  return CANCEL_AND_IGNORE;
}

}  // namespace pdf
