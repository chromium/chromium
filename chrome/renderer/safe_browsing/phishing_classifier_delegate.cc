// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_classifier_delegate.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/renderer/safe_browsing/feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/phishing_classifier.h"
#include "chrome/renderer/safe_browsing/scorer.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using content::DocumentState;
using content::RenderThread;

namespace safe_browsing {

static GURL StripRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

typedef std::set<PhishingClassifierDelegate*> PhishingClassifierDelegates;
static base::LazyInstance<PhishingClassifierDelegates>::DestructorAtExit
    g_delegates = LAZY_INSTANCE_INITIALIZER;

static base::LazyInstance<std::unique_ptr<const safe_browsing::Scorer>>::
    DestructorAtExit g_phishing_scorer = LAZY_INSTANCE_INITIALIZER;

// static
void PhishingClassifierFilter::Create(
    mojom::PhishingModelSetterRequest request) {
  mojo::MakeStrongBinding(std::make_unique<PhishingClassifierFilter>(),
                          std::move(request));
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
  PhishingClassifierDelegates::iterator i;
  for (i = g_delegates.Get().begin(); i != g_delegates.Get().end(); ++i) {
    (*i)->SetPhishingScorer(scorer);
  }
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
  g_delegates.Get().insert(this);
  if (!classifier) {
    classifier =
        new PhishingClassifier(render_frame, new FeatureExtractorClock());
  }

  classifier_.reset(classifier);

  if (g_phishing_scorer.Get().get())
    SetPhishingScorer(g_phishing_scorer.Get().get());

  registry_.AddInterface(
      base::BindRepeating(&PhishingClassifierDelegate::PhishingDetectorRequest,
                          base::Unretained(this)));
}

PhishingClassifierDelegate::~PhishingClassifierDelegate() {
  CancelPendingClassification(SHUTDOWN);
  g_delegates.Get().erase(this);
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
  // Start classifying the current page if all conditions are met.
  // See MaybeStartClassification() for details.
  MaybeStartClassification();
}

void PhishingClassifierDelegate::PhishingDetectorRequest(
    mojom::PhishingDetectorRequest request) {
  phishing_detector_bindings_.AddBinding(this, std::move(request));
}

void PhishingClassifierDelegate::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void PhishingClassifierDelegate::StartPhishingDetection(const GURL& url) {
  last_url_received_from_browser_ = StripRef(url);
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
  // We no longer need the page text.
  classifier_page_text_.clear();
  DVLOG(2) << "Phishy verdict = " << verdict.is_phishing()
           << " score = " << verdict.client_score();
  if (verdict.client_score() != PhishingClassifier::kInvalidScore) {
    DCHECK_EQ(last_url_sent_to_classifier_.spec(), verdict.url());
    safe_browsing::mojom::PhishingDetectorClientPtr phishing_detector;
    render_frame()->GetRemoteInterfaces()->GetInterface(&phishing_detector);
    phishing_detector->PhishingDetectionDone(verdict.SerializeAsString());
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
    return;
  }

  GURL stripped_last_load_url(StripRef(last_finished_load_url_));
  if (stripped_last_load_url == StripRef(last_url_sent_to_classifier_)) {
    // We've already classified this toplevel URL, so this was likely an
    // same-document navigation or a subframe navigation.  The browser should
    // not send a StartPhishingDetection IPC in this case.
    DVLOG(2) << "Toplevel URL is unchanged, not starting classification.";
    classifier_page_text_.clear();  // we won't need this.
    have_page_text_ = false;
    return;
  }

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
      base::Bind(&PhishingClassifierDelegate::ClassificationDone,
                 base::Unretained(this)));
}

void PhishingClassifierDelegate::OnDestruct() {
  delete this;
}

}  // namespace safe_browsing
