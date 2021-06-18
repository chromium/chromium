// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_navigation_throttle.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/common/constants.h"
#include "pdf/pdf_features.h"
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
  }

 private:
  friend class content::WebContentsUserData<WebContentsLifetimeHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit WebContentsLifetimeHelper(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  content::WebContents* web_contents_;
  base::WeakPtrFactory<WebContentsLifetimeHelper> weak_factory_{this};
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsLifetimeHelper)

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
PdfNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  if (!base::FeatureList::IsEnabled(chrome_pdf::features::kPdfUnseasoned))
    return nullptr;

  if (navigation_handle->IsInMainFrame())
    return nullptr;

  return std::make_unique<PdfNavigationThrottle>(navigation_handle);
}

PdfNavigationThrottle::PdfNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

PdfNavigationThrottle::~PdfNavigationThrottle() = default;

const char* PdfNavigationThrottle::GetNameForLogging() {
  return "PdfNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
PdfNavigationThrottle::WillStartRequest() {
  // Quickly ignore URLs that do not look like stream URLs. The path check does
  // not need to be precise, it just needs to not match any legitimate PDF
  // extension URL. We'll assume such URLs contain multiple path components, or
  // a file extension.
  const GURL& url = navigation_handle()->GetURL();
  if (!url.SchemeIs(extensions::kExtensionScheme) ||
      url.host_piece() != extension_misc::kPdfExtensionId ||
      url.path_piece().find_last_of("/.") != 0) {
    return PROCEED;
  }

  // TODO(crbug.com/1123621): Enqueue navigation to stream's original URL.
  content::OpenURLParams params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  params.url = GURL("chrome://about");
  params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;

  // Uses same pattern as `PDFIFrameNavigationThrottle` to redirect navigation.
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents)
    return CANCEL_AND_IGNORE;

  WebContentsLifetimeHelper::CreateForWebContents(web_contents);
  WebContentsLifetimeHelper* helper =
      WebContentsLifetimeHelper::FromWebContents(web_contents);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&WebContentsLifetimeHelper::OpenUrl,
                                helper->GetWeakPtr(), std::move(params)));
  return CANCEL_AND_IGNORE;
}

}  // namespace pdf
