// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_classifier_delegate.h"

#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-forward.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_classifier.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/scorer.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

using content::RenderThread;

namespace safe_browsing {

namespace {

GURL StripRef(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

void LogClassificationRetryWithinTimeout(bool success) {
  base::UmaHistogramBoolean(
      "SBClientPhishing.Classifier.ReadyAfterRetryTimeout", success);
}

std::string GetRequestTypeName(
    safe_browsing::mojom::ClientSideDetectionType client_side_detection_type) {
  switch (client_side_detection_type) {
    case safe_browsing::mojom::ClientSideDetectionType::kForceRequest:
      return "ForceRequest";
    case safe_browsing::mojom::ClientSideDetectionType::
        kNotificationPermissionPrompt:
      return "NotificationPermissionPrompt";
    case safe_browsing::mojom::ClientSideDetectionType::kTriggerModels:
      return "TriggerModel";
    case safe_browsing::mojom::ClientSideDetectionType::kKeyboardLock:
      return "KeyboardLockRequested";
    case safe_browsing::mojom::ClientSideDetectionType::kPointerLock:
      return "PointerLockRequested";
    case safe_browsing::mojom::ClientSideDetectionType::kVibrationApi:
      return "VibrationApi";
    case safe_browsing::mojom::ClientSideDetectionType::kFullscreen:
      return "FullscreenApi";
    case safe_browsing::mojom::ClientSideDetectionType::kPasswordProtection:
      return "PasswordProtection";
    case safe_browsing::mojom::ClientSideDetectionType::kClipboardCopyApi:
      return "ClipboardCopyApi";
    case safe_browsing::mojom::ClientSideDetectionType::kCreditCardForm:
      return "CreditCardForm";
  }
}

}  // namespace

PhishingClassifierDelegate::PhishingClassifierDelegate(
    content::RenderFrame* render_frame,
    PhishingClassifier* classifier)
    : content::RenderFrameObserver(render_frame),
      last_main_frame_transition_(ui::PAGE_TRANSITION_LINK),
      is_classifying_(false),
      awaiting_retry_(false) {
  if (!classifier) {
    classifier = new PhishingClassifier(render_frame);
  }

  classifier_.reset(classifier);

  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::PhishingDetector>(base::BindRepeating(
          &PhishingClassifierDelegate::PhishingDetectorReceiver,
          base::Unretained(this)));

  model_change_observation_.Observe(ScorerStorage::GetInstance());
}

PhishingClassifierDelegate::~PhishingClassifierDelegate() {
  CancelPendingClassification(CancelClassificationReason::kShutdown);
}

// static
PhishingClassifierDelegate* PhishingClassifierDelegate::Create(
    content::RenderFrame* render_frame,
    PhishingClassifier* classifier) {
  // Private constructor and public static Create() method to facilitate
  // stubbing out this class for binary-size reduction purposes.
  return new PhishingClassifierDelegate(render_frame, classifier);
}

void PhishingClassifierDelegate::PhishingDetectorReceiver(
    mojo::PendingAssociatedReceiver<mojom::PhishingDetector> receiver) {
  phishing_detector_receiver_.reset();
  phishing_detector_receiver_.Bind(std::move(receiver));
}

void PhishingClassifierDelegate::StartPhishingDetection(
    const GURL& url,
    safe_browsing::mojom::ClientSideDetectionType request_type,
    StartPhishingDetectionCallback callback) {
  if (!callback_.is_null())
    std::move(callback_).Run(mojom::PhishingDetectorResult::CANCELLED,
                             std::nullopt);
  is_phishing_detection_running_ = true;
  awaiting_retry_ = false;
  last_url_received_from_browser_ = StripRef(url);
  callback_ = std::move(callback);
  request_type_ = request_type;
  RecordEvent(SBPhishingClassifierEvent::kPhishingDetectionRequested);
  // Start classifying the current page if all conditions are met.
  // See MaybeStartClassification() for details.
  MaybeStartClassification();
}

void PhishingClassifierDelegate::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // A new page is starting to load, so cancel classificaiton.
  CancelPendingClassification(CancelClassificationReason::kNavigateAway);
  if (!frame->Parent())
    last_main_frame_transition_ = transition;
}

bool PhishingClassifierDelegate::is_ready() {
  return classifier_->is_ready();
}

void PhishingClassifierDelegate::PageCaptured(
    scoped_refptr<const base::RefCountedString16> page_text,
    bool preliminary_capture) {
  RecordEvent(SBPhishingClassifierEvent::kPageTextCaptured);

  if (preliminary_capture) {
    return;
  }

  // Note: Currently, if the url hasn't changed, we won't restart
  // classification in this case.  We may want to adjust this.

  last_finished_load_url_ = render_frame()->GetWebFrame()->GetDocument().Url();

  GURL stripped_last_load_url(StripRef(last_finished_load_url_));
  // Check if toplevel URL has changed.
  if (stripped_last_load_url == StripRef(last_url_sent_to_classifier_)) {
    return;
  }

  MaybeStartClassification();
}

void PhishingClassifierDelegate::CancelPendingClassification(
    CancelClassificationReason reason) {
  if (is_classifying_) {
    is_classifying_ = false;
    base::UmaHistogramEnumeration("SBClientPhishing.CancelClassificationReason",
                                  reason);
    if (request_type_.has_value()) {
      base::UmaHistogramEnumeration(
          "SBClientPhishing.CancelClassificationReason." +
              GetRequestTypeName(request_type_.value()),
          reason);
    }
  }
  if (classifier_->is_ready()) {
    classifier_->CancelPendingClassification();
  }
  awaiting_retry_ = false;
  request_type_ = std::nullopt;
}

void PhishingClassifierDelegate::ClassificationDone(
    const ClientPhishingRequest& verdict,
    PhishingClassifier::Result phishing_classifier_result) {
  RecordEvent(SBPhishingClassifierEvent::kClassificationComplete);
  is_phishing_detection_running_ = false;
  if (callback_.is_null()) {
    RecordEvent(
        SBPhishingClassifierEvent::kPhishingClasifierCallbackEmptyOnCompletion);
    return;
  }

  mojom::PhishingDetectorResult result = mojom::PhishingDetectorResult::SUCCESS;

  if (verdict.client_score() == PhishingClassifier::kClassifierFailed) {
    switch (phishing_classifier_result) {
      case PhishingClassifier::Result::kInvalidScore:
        result = mojom::PhishingDetectorResult::INVALID_SCORE;
        break;
      case PhishingClassifier::Result::kInvalidURLFormatRequest:
        result = mojom::PhishingDetectorResult::INVALID_URL_FORMAT_REQUEST;
        break;
      case PhishingClassifier::Result::kInvalidDocumentLoader:
        result = mojom::PhishingDetectorResult::INVALID_DOCUMENT_LOADER;
        break;
      case PhishingClassifier::Result::kURLFeatureExtractionFailed:
        result = mojom::PhishingDetectorResult::URL_FEATURE_EXTRACTION_FAILED;
        break;
      case PhishingClassifier::Result::kDOMExtractionFailed:
        result = mojom::PhishingDetectorResult::DOM_EXTRACTION_FAILED;
        break;
      case PhishingClassifier::Result::kTermExtractionFailed:
        result = mojom::PhishingDetectorResult::TERM_EXTRACTION_FAILED;
        break;
      case PhishingClassifier::Result::kVisualExtractionFailed:
        result = mojom::PhishingDetectorResult::VISUAL_EXTRACTION_FAILED;
        break;
      default:
        NOTREACHED();
    }
  }

  if (result == mojom::PhishingDetectorResult::SUCCESS) {
    DCHECK_EQ(last_url_sent_to_classifier_.spec(), verdict.url());
  }
  request_type_ = std::nullopt;
  RecordEvent(SBPhishingClassifierEvent::kPhishingClassifierRequestResponded);
  std::move(callback_).Run(result, mojo_base::ProtoWrapper(verdict));
}

void PhishingClassifierDelegate::MaybeStartClassification() {
  // We can begin phishing classification when the following conditions are
  // met:
  //  1. A Scorer has been created
  //  2. The browser has sent a StartPhishingDetection message for the
  //     current toplevel URL.
  //  3. The page has finished loading.
  //  4. The load is a new navigation (not a session history navigation).
  //  5. The toplevel URL has not already been classified.
  //
  // Note that if we determine that this particular navigation should not be
  // classified at all (as opposed to deferring it until we get an IPC or
  // the load completes), we discard the page text since it won't be needed.
  if (!classifier_->is_ready()) {
    // We should only retry if a phishing detection has been requested, which
    // is tracked by |is_phishing_detection_running_|.
    if (base::FeatureList::IsEnabled(kClientSideDetectionRetryLimit) &&
        is_phishing_detection_running_ && !awaiting_retry_) {
      awaiting_retry_ = true;

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PhishingClassifierDelegate::OnRetryTimeout,
                         weak_factory_.GetWeakPtr()),
          base::Seconds(kClientSideDetectionRetryLimitTime.Get()));
    } else {
      is_phishing_detection_running_ = false;
      if (!callback_.is_null()) {
        std::move(callback_).Run(
            mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY, std::nullopt);
      }
    }
    return;
  }

  if (last_main_frame_transition_ & ui::PAGE_TRANSITION_FORWARD_BACK) {
    // Skip loads from session history navigation.  However, update the
    // last URL sent to the classifier, so that we'll properly detect
    // same-document navigations.
    last_url_sent_to_classifier_ = last_finished_load_url_;
    is_phishing_detection_running_ = false;
    if (!callback_.is_null())
      std::move(callback_).Run(
          mojom::PhishingDetectorResult::FORWARD_BACK_TRANSITION, std::nullopt);
    return;
  }

  GURL stripped_last_load_url(StripRef(last_finished_load_url_));
  if (last_url_received_from_browser_ != stripped_last_load_url) {
    bool match_on_stripped_empty_path =
        last_url_received_from_browser_.GetWithEmptyPath() ==
        stripped_last_load_url.GetWithEmptyPath();
    base::UmaHistogramBoolean(
        "SBClientPhishing.PhishingClassifierMatchOnStrippedEmptyPath",
        match_on_stripped_empty_path);
    if (request_type_.has_value()) {
      base::UmaHistogramBoolean(
          "SBClientPhishing.PhishingClassifierMatchOnStrippedEmptyPath." +
              GetRequestTypeName(request_type_.value()),
          match_on_stripped_empty_path);
    }
    RecordEvent(SBPhishingClassifierEvent::kUrlShouldNotBeClassified);
    // The browser has not yet confirmed that this URL should be classified,
    // so defer classification for now.  Note: the ref does not affect
    // any of the browser's preclassification checks, so we don't require it
    // to match.
    return;
  }

  last_url_sent_to_classifier_ = last_finished_load_url_;

  if (awaiting_retry_) {
    LogClassificationRetryWithinTimeout(true);
    awaiting_retry_ = false;
  }

  is_classifying_ = true;
  RecordEvent(SBPhishingClassifierEvent::kClassificationBegin);
  classifier_->BeginClassification(base::BindOnce(
      &PhishingClassifierDelegate::ClassificationDone, base::Unretained(this)));
}

void PhishingClassifierDelegate::OnRetryTimeout() {
  // If |awaiting_retry_| is false, the classification is happening, completed,
  // cancelled, or there is a new phishing detection request.
  if (!awaiting_retry_) {
    return;
  }

  is_phishing_detection_running_ = false;
  awaiting_retry_ = false;
  if (!callback_.is_null()) {
    std::move(callback_).Run(
        mojom::PhishingDetectorResult::CLASSIFIER_NOT_READY, std::nullopt);
  }
  LogClassificationRetryWithinTimeout(false);
}

void PhishingClassifierDelegate::RecordEvent(SBPhishingClassifierEvent event) {
  base::UmaHistogramEnumeration("SBClientPhishing.Classifier.Event", event);
  if (request_type_.has_value()) {
    base::UmaHistogramEnumeration("SBClientPhishing.Classifier.Event." +
                                      GetRequestTypeName(request_type_.value()),
                                  event);
  }
}

void PhishingClassifierDelegate::OnDestruct() {
  if (is_phishing_detection_running_) {
    RecordEvent(SBPhishingClassifierEvent::kDestructedBeforeClassificationDone);
  }
  delete this;
}

void PhishingClassifierDelegate::OnScorerChanged() {
  Scorer* scorer = ScorerStorage::GetInstance()->GetScorer();

  if (!scorer) {
    // If the scorer is reset, we should clear pending classification if there
    // is one going on, which is checked by the function below.
    CancelPendingClassification(CancelClassificationReason::kScorerCleared);
    return;
  }

  // We check |is_classifying_| here because |CancelPendingClassification|
  // clears the page text, and we do not want that if we are awaiting retry.
  if (is_classifying_) {
    CancelPendingClassification(
        CancelClassificationReason::kNewPhishingScorerUpdate);
  } else if (awaiting_retry_) {
    // If a classificiation is not going on right now, a retry has been
    // attempted, and we're still within the timeout, call the classification
    // process again, because we should be able to classify now with the scorer
    // available.
    RecordEvent(SBPhishingClassifierEvent::kScorerUpdatedWithinRetryTimeout);
    MaybeStartClassification();
  }
}

}  // namespace safe_browsing
