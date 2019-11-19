// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/stack_trace.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "chrome/renderer/safe_browsing/feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/phishing_classifier.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "components/safe_browsing/common/safe_browsing.mojom-forward.h"
#include "components/safe_browsing/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using content::DocumentState;
using content::RenderThread;

namespace safe_browsing {

namespace {

GURL StripRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

std::set<PhishingClassifierDelegate*>& PhishingClassifierDelegates() {
  static base::NoDestructor<std::set<PhishingClassifierDelegate*>> s;
  return *s;
}

base::LazyInstance<std::unique_ptr<const safe_browsing::Scorer>>::
    DestructorAtExit g_phishing_scorer = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void PhishingClassifierFilter::Create(
    mojo::PendingReceiver<mojom::PhishingModelSetter> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<PhishingClassifierFilter>(),
                              std::move(receiver));
}

PhishingClassifierFilter::PhishingClassifierFilter() {}

PhishingClassifierFilter::~PhishingClassifierFilter() {}

void PhishingClassifierFilter::SetPhishingModel(const std::string& model) {
  safe_browsing::Scorer* scorer = NULL;
  // An empty model string means we should disable client-side phishing
  // detection.
  if (!model.empty()) {
    scorer = safe_browsing::Scorer::Create(model);
    if (!scorer) {
      DLOG(ERROR) << "Unable to create a PhishingScorer - corrupt model?";
      return;
    }
  }
  for (auto* delegate : PhishingClassifierDelegates())
    delegate->SetPhishingScorer(scorer);
  g_phishing_scorer.Get().reset(scorer);
}

// static
PhishingClassifierDelegate* PhishingClassifierDelegate::Create(
    content::RenderFrame* render_frame,
    PhishingClassifier* classifier) {
  // Private constructor and public static Create() method to facilitate
  // stubbing out this class for binary-size reduction purposes.
  return new PhishingClassifierDelegate(render_frame, classifier);
}

PhishingClassifierDelegate::PhishingClassifierDelegate(
    content::RenderFrame* render_frame,
    PhishingClassifier* classifier)
    : content::RenderFrameObserver(render_frame),
      last_main_frame_transition_(ui::PAGE_TRANSITION_LINK),
      have_page_text_(false),
      is_classifying_(false) {
  PhishingClassifierDelegates().insert(this);
  if (!classifier) {
    classifier =
        new PhishingClassifier(render_frame, new FeatureExtractorClock());
  }

  classifier_.reset(classifier);

  if (g_phishing_scorer.Get().get())
    SetPhishingScorer(g_phishing_scorer.Get().get());

  registry_.AddInterface(
      base::BindRepeating(&PhishingClassifierDelegate::PhishingDetectorReceiver,
                          base::Unretained(this)));
}

PhishingClassifierDelegate::~PhishingClassifierDelegate() {
  CancelPendingClassification(SHUTDOWN);
  PhishingClassifierDelegates().erase(this);
}

void PhishingClassifierDelegate::SetPhishingScorer(
    const safe_browsing::Scorer* scorer) {
  if (is_classifying_) {
    // If there is a classification going on right now it means we're
    // actually replacing an existing scorer with a new model.  In
    // this case we simply cancel the current classification.
    // TODO(noelutz): if this happens too frequently we could also
    // replace the old scorer with the new one once classification is done
    // but this would complicate the code somewhat.
    CancelPendingClassification(NEW_PHISHING_SCORER);
  }
  classifier_->set_phishing_scorer(scorer);
}

void PhishingClassifierDelegate::PhishingDetectorReceiver(
    mojo::PendingReceiver<mojom::PhishingDetector> receiver) {
  phishing_detector_receivers_.Add(this, std::move(receiver));
}

void PhishingClassifierDelegate::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void PhishingClassifierDelegate::StartPhishingDetection(
    const GURL& url,
    StartPhishingDetectionCallback callback) {
  if (!callback_.is_null())
    std::move(callback_).Run(mojom::PhishingDetectorResult::CANCELLED, "");

  last_url_received_from_browser_ = StripRef(url);
  callback_ = std::move(callback);
  // Start classifying the current page if all conditions are met.
  // See MaybeStartClassification() for details.
  MaybeStartClassification();
}

void PhishingClassifierDelegate::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // A new page is starting to load, so cancel classificaiton.
  //
  // TODO(bryner): We shouldn't need to cancel classification if the navigation
  // is within the same document.  However, if we let classification continue in
  // this case, we need to properly deal with the fact that PageCaptured will
  // be called again for the same-document navigation.  We need to be sure not
  // to swap out the page text while the term feature extractor is still
  // running.
  CancelPendingClassification(is_same_document_navigation ? NAVIGATE_WITHIN_PAGE
                                                          : NAVIGATE_AWAY);
  if (frame->Parent())
    return;

  last_main_frame_transition_ = transition;
}

void PhishingClassifierDelegate::PageCaptured(base::string16* page_text,
                                              bool preliminary_capture) {
  if (preliminary_capture) {
    return;
  }
  // Make sure there's no classification in progress.  We don't want to swap
  // out the page text string from underneath the term feature extractor.
  //
  // Note: Currently, if the url hasn't changed, we won't restart
  // classification in this case.  We may want to adjust this.
  CancelPendingClassification(PAGE_RECAPTURED);
  last_finished_load_url_ = render_frame()->GetWebFrame()->GetDocument().Url();
  classifier_page_text_.swap(*page_text);
  have_page_text_ = true;

  GURL stripped_last_load_url(StripRef(last_finished_load_url_));
  if (stripped_last_load_url == StripRef(last_url_sent_to_classifier_)) {
    DVLOG(2) << "Toplevel URL is unchanged, not starting classification.";
    return;
  }

  UMA_HISTOGRAM_BOOLEAN(
      "SBClientPhishing.PageCapturedMatchesBrowserURL",
      (last_url_received_from_browser_ == stripped_last_load_url));

  MaybeStartClassification();
}

void PhishingClassifierDelegate::CancelPendingClassification(
    CancelClassificationReason reason) {
  if (is_classifying_) {
    UMA_HISTOGRAM_ENUMERATION("SBClientPhishing.CancelClassificationReason",
                              reason,
                              CANCEL_CLASSIFICATION_MAX);
    is_classifying_ = false;
  }
  if (classifier_->is_ready()) {
    classifier_->CancelPendingClassification();
  }
  classifier_page_text_.clear();
  have_page_text_ = false;
}

void PhishingClassifierDelegate::ClassificationDone(
    const ClientPhishingRequest& verdict) {
  DVLOG(2) << "Phishy verdict = " << verdict.is_phishing()
           << " score = " << verdict.client_score();
  if (callback_.is_null())
    return;

  if (verdict.client_score() != PhishingClassifier::kInvalidScore) {
    DCHECK_EQ(last_url_sent_to_classifier_.spec(), verdict.url());
    std::move(callback_).Run(mojom::PhishingDetectorResult::SUCCESS,
                             verdict.SerializeAsString());
  } else {
    std::move(callback_).Run(mojom::PhishingDetectorResult::INVALID_SCORE,
                             verdict.SerializeAsString());
  }
}

void PhishingClassifierDelegate::MaybeStartClassification() {
  // We can begin phishing classification when the following conditions are
  // met:
  //  1. A Scorer has been created
  //  2. The browser has sent a StartPhishingDetection message for the current
  //     toplevel URL.
  //  3. The page has finished loading and the page text has been extracted.
  //  4. The load is a new navigation (not a session history navigation).
  //  5. The toplevel URL has not already been classified.
  //
  // Note that if we determine that this particular navigation should not be
  // classified at all (as opposed to deferring it until we get an IPC or the
  // load completes), we discard the page text since it won't be needed.
  if (!classifier_->is_ready()) {
    DVLOG(2) << "Not starting classification, no Scorer created.";
    // Keep classifier_page_text_, in case a Scorer is set later.
    if (!callback_.is_null())
      std::move(callback_).Run(
          mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY, "");
    return;
  }

  if (last_main_frame_transition_ & ui::PAGE_TRANSITION_FORWARD_BACK) {
    // Skip loads from session history navigation.  However, update the
    // last URL sent to the classifier, so that we'll properly detect
    // same-document navigations.
    DVLOG(2) << "Not starting classification for back/forward navigation";
    last_url_sent_to_classifier_ = last_finished_load_url_;
    classifier_page_text_.clear();  // we won't need this.
    have_page_text_ = false;
    if (!callback_.is_null())
      std::move(callback_).Run(
          mojom::PhishingDetectorResult::FORWARD_BACK_TRANSITION, "");
    return;
  }

  GURL stripped_last_load_url(StripRef(last_finished_load_url_));
  if (!have_page_text_) {
    DVLOG(2) << "Not starting classification, there is no page text ready.";
    return;
  }

  if (last_url_received_from_browser_ != stripped_last_load_url) {
    // The browser has not yet confirmed that this URL should be classified,
    // so defer classification for now.  Note: the ref does not affect
    // any of the browser's preclassification checks, so we don't require it
    // to match.
    DVLOG(2) << "Not starting classification, last url from browser is "
             << last_url_received_from_browser_ << ", last finished load is "
             << last_finished_load_url_;
    // Keep classifier_page_text_, in case the browser notifies us later that
    // we should classify the URL.
    return;
  }

  DVLOG(2) << "Starting classification for " << last_finished_load_url_;
  last_url_sent_to_classifier_ = last_finished_load_url_;
  is_classifying_ = true;
  classifier_->BeginClassification(
      &classifier_page_text_,
      base::BindOnce(&PhishingClassifierDelegate::ClassificationDone,
                     base::Unretained(this)));
}

void PhishingClassifierDelegate::OnDestruct() {
  delete this;
}

}  // namespace safe_browsing
