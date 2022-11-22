// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_navigation_throttle.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/pdf/browser/pdf_stream_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace pdf {

// static
std::unique_ptr<content::NavigationThrottle>
PdfNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<PdfStreamDelegate> stream_delegate) {
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

  // Reset the source SiteInstance.  This is a workaround for a lifetime bug:
  // leaving the source SiteInstance in OpenURLParams could inadvertently
  // prolong the SiteInstance's lifetime beyond the lifetime of the
  // BrowserContext it's associated with.  The BrowserContext could get
  // destroyed after the task below is scheduled but before it runs (see
  // https://crbug.com/1382761), and even though the task uses a WebContents
  // WeakPtr to return early in that case, the task's OpenURLParams would only
  // get destroyed and decrement the source SiteInstance's refcount at the time
  // of that early return, which is already after the BrowserContext is
  // destroyed.  This can cause logic in the SiteInstance destructor to trip up
  // if it tries to use the SiteInstance's BrowserContext.
  //
  // Fortunately, the source SiteInstance of this navigation should always
  // correspond to the PDF extension loaded in the primary main frame of
  // `contents`. Hence, if the navigation task does run and does not get
  // canceled due to WebContents becoming null, we can restore the source
  // SiteInstance at that point.
  //
  // TODO(crbug.com/1382761): This should be fixed in a more systematic way.
  DCHECK_EQ(params.source_site_instance,
            contents->GetPrimaryMainFrame()->GetSiteInstance());
  params.source_site_instance.reset();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> web_contents,
             const content::OpenURLParams& params) {
            if (!web_contents)
              return;

            // Restore the source SiteInstance that was cleared out of the
            // original OpenURLParams.
            content::OpenURLParams new_params = params;
            new_params.source_site_instance =
                web_contents->GetPrimaryMainFrame()->GetSiteInstance();

            // `MimeHandlerViewGuest` navigates its embedder for calls to
            // `WebContents::OpenURL()`, so use `LoadURLWithParams()` directly
            // instead.
            web_contents->GetController().LoadURLWithParams(
                content::NavigationController::LoadURLParams(new_params));

            // Note that we don't need to register the stream's URL loader as a
            // subresource, as `MimeHandlerViewGuest::ReadyToCommitNavigation()`
            // will handle this as soon as we navigate to a
            // non-`kPdfExtensionId` URL.
          },
          contents->GetWeakPtr(), std::move(params)));
  return CANCEL_AND_IGNORE;
}

}  // namespace pdf
