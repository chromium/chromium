// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_dom_feature_extractor.h"

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace safe_browsing {

// This time should be short enough that it doesn't noticeably disrupt the
// user's interaction with the page.
const int PhishingDOMFeatureExtractor::kMaxTimePerChunkMs = 10;

// Experimenting shows that we get a reasonable gain in performance by
// increasing this up to around 10, but there's not much benefit in
// increasing it past that.
const int PhishingDOMFeatureExtractor::kClockCheckGranularity = 10;

// This should be longer than we expect feature extraction to take on any
// actual phishing page.
const int PhishingDOMFeatureExtractor::kMaxTotalTimeMs = 500;

// Intermediate state used for computing features.  See features.h for
// descriptions of the DOM features that are computed.
struct PhishingDOMFeatureExtractor::PageFeatureState {
  // Link related features
  int external_links;
  std::unordered_set<std::string> external_domains;
  int secure_links;
  int total_links;

  // Form related features
  int num_forms;
  int num_text_inputs;
  int num_pswd_inputs;
  int num_radio_inputs;
  int num_check_inputs;
  int action_other_domain;
  int total_actions;
  std::unordered_set<std::string> page_action_urls;

  // Image related features
  int img_other_domain;
  int total_imgs;

  // How many script tags
  int num_script_tags;

  // The time at which we started feature extraction for the current page.
  base::TimeTicks start_time;

  // The number of iterations we've done for the current extraction.
  int num_iterations;

  explicit PageFeatureState(base::TimeTicks start_time_ticks)
      : external_links(0),
        secure_links(0),
        total_links(0),
        num_forms(0),
        num_text_inputs(0),
        num_pswd_inputs(0),
        num_radio_inputs(0),
        num_check_inputs(0),
        action_other_domain(0),
        total_actions(0),
        img_other_domain(0),
        total_imgs(0),
        num_script_tags(0),
        start_time(start_time_ticks),
        num_iterations(0) {}

  ~PageFeatureState() {}
};

// Per-frame state
struct PhishingDOMFeatureExtractor::FrameData {
  // This is our reference to document.all, which is an iterator over all
  // of the elements in the document.  It keeps track of our current position.
  blink::WebElementCollection elements;
  // The domain of the document URL, stored here so that we don't need to
  // recompute it every time it's needed.
  std::string domain;
};

PhishingDOMFeatureExtractor::PhishingDOMFeatureExtractor()
    : clock_(base::DefaultTickClock::GetInstance()) {
  Clear();
}

PhishingDOMFeatureExtractor::~PhishingDOMFeatureExtractor() {
  CancelPendingExtraction();
}

void PhishingDOMFeatureExtractor::ExtractFeatures(blink::WebDocument document,
                                                  FeatureMap* features,
                                                  DoneCallback done_callback) {
  // The RenderView should have called CancelPendingExtraction() before
  // starting a new extraction, so DCHECK this.
  DCHECK(done_callback_.is_null());
  DCHECK(!cur_frame_data_.get());
  DCHECK(cur_document_.IsNull());
  // However, in an opt build, we will go ahead and clean up the pending
  // extraction so that we can start in a known state.
  CancelPendingExtraction();

  features_ = features;
  done_callback_ = std::move(done_callback);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("safe_browsing", "ExtractDomFeatures",
                                    this);
  page_feature_state_ = std::make_unique<PageFeatureState>(clock_->NowTicks());
  cur_document_ = document;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PhishingDOMFeatureExtractor::ExtractFeaturesWithTimeout,
                     weak_factory_.GetWeakPtr()));
}

void PhishingDOMFeatureExtractor::CancelPendingExtraction() {
  // Cancel any pending callbacks, and clear our state.
  weak_factory_.InvalidateWeakPtrs();
  Clear();
}

void PhishingDOMFeatureExtractor::ExtractFeaturesWithTimeout() {
  DCHECK(page_feature_state_.get());
  ++page_feature_state_->num_iterations;
  base::TimeTicks current_chunk_start_time = clock_->NowTicks();

  if (cur_document_.IsNull()) {
    // This will only happen if we weren't able to get the document for the
    // main frame.  We'll treat this as an extraction failure.
    RunCallback(false);
    return;
  }

  int num_elements = 0;
  for (; !cur_document_.IsNull(); cur_document_ = GetNextDocument()) {
    blink::WebElement cur_element;
    if (cur_frame_data_.get()) {
      // We're resuming traversal of a frame, so just advance to the next
      // element.
      cur_element = cur_frame_data_->elements.NextItem();
      // When we resume the traversal, the first call to nextItem() potentially
      // has to walk through the document again from the beginning, if it was
      // modified between our chunks of work.
    } else {
      // We just moved to a new frame, so update our frame state
      // and advance to the first element.
      ResetFrameData();
      cur_element = cur_frame_data_->elements.FirstItem();
    }

    for (; !cur_element.IsNull();
         cur_element = cur_frame_data_->elements.NextItem()) {
      if (cur_element.HasHTMLTagName("a")) {
        HandleLink(cur_element);
      } else if (cur_element.HasHTMLTagName("form")) {
        HandleForm(cur_element);
      } else if (cur_element.HasHTMLTagName("img")) {
        HandleImage(cur_element);
      } else if (cur_element.HasHTMLTagName("input")) {
        HandleInput(cur_element);
      } else if (cur_element.HasHTMLTagName("script")) {
        HandleScript(cur_element);
      }

      if (++num_elements >= kClockCheckGranularity) {
        num_elements = 0;
        base::TimeTicks now = clock_->NowTicks();
        if (now - page_feature_state_->start_time >=
            base::Milliseconds(kMaxTotalTimeMs)) {
          RunCallback(false);
          return;
        }
        base::TimeDelta chunk_elapsed = now - current_chunk_start_time;
        if (chunk_elapsed >= base::Milliseconds(kMaxTimePerChunkMs)) {
          // The time limit for the current chunk is up, so post a task to
          // continue extraction.
          //
          // Record how much time we actually spent on the chunk. If this is
          // much higher than kMaxTimePerChunkMs, we may need to adjust the
          // clock granularity.
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &PhishingDOMFeatureExtractor::ExtractFeaturesWithTimeout,
                  weak_factory_.GetWeakPtr()));
          return;
        }
        // Otherwise, continue.
      }
    }

    // We're done with this frame, recalculate the FrameData when we
    // advance to the next frame.
    cur_frame_data_.reset();
  }

  InsertFeatures();
  RunCallback(true);
}

void PhishingDOMFeatureExtractor::HandleLink(const blink::WebElement& element) {
  // Count the number of times we link to a different host.
  if (!element.HasAttribute("href"))
    return;

  // Retrieve the link and resolve the link in case it's relative.
  blink::WebURL full_url = CompleteURL(element, element.GetAttribute("href"));

  std::string domain;
  bool is_external = IsExternalDomain(full_url, &domain);
  if (domain.empty())
    return;

  if (is_external) {
    ++page_feature_state_->external_links;

    // Record each unique domain that we link to.
    page_feature_state_->external_domains.insert(domain);
  }

  // Check how many are https links.
  if (GURL(full_url).SchemeIs("https")) {
    ++page_feature_state_->secure_links;
  }

  ++page_feature_state_->total_links;
}

void PhishingDOMFeatureExtractor::HandleForm(const blink::WebElement& element) {
  // Increment the number of forms on this page.
  ++page_feature_state_->num_forms;

  // Record whether the action points to a different domain.
  if (!element.HasAttribute("action")) {
    return;
  }

  blink::WebURL full_url = CompleteURL(element, element.GetAttribute("action"));

  page_feature_state_->page_action_urls.insert(full_url.GetString().Utf8());

  std::string domain;
  bool is_external = IsExternalDomain(full_url, &domain);
  if (domain.empty())
    return;

  if (is_external) {
    ++page_feature_state_->action_other_domain;
  }
  ++page_feature_state_->total_actions;
}

void PhishingDOMFeatureExtractor::HandleImage(
    const blink::WebElement& element) {
  // Record whether the image points to a different domain.
  blink::WebURL full_url = CompleteURL(element, element.GetAttribute("src"));
  std::string domain;
  bool is_external = IsExternalDomain(full_url, &domain);
  if (domain.empty())
    return;

  if (is_external)
    ++page_feature_state_->img_other_domain;

  ++page_feature_state_->total_imgs;
}

void PhishingDOMFeatureExtractor::HandleInput(
    const blink::WebElement& element) {
  // The HTML spec says that if the type is unspecified, it defaults to text.
  // In addition, any unrecognized type will be treated as a text input.
  //
  // Note that we use the attribute value rather than
  // WebFormControlElement::formControlType() for consistency with the
  // way the phishing classification model is created.
  std::string type = base::ToLowerASCII(element.GetAttribute("type").Utf8());
  if (type == "password") {
    ++page_feature_state_->num_pswd_inputs;
  } else if (type == "radio") {
    ++page_feature_state_->num_radio_inputs;
  } else if (type == "checkbox") {
    ++page_feature_state_->num_check_inputs;
  } else if (type != "submit" && type != "reset" && type != "file" &&
             type != "hidden" && type != "image" && type != "button") {
    // Note that there are a number of new input types in HTML5 that are not
    // handled above.  For now, we will consider these as text inputs since
    // they could be used to capture user input.
    ++page_feature_state_->num_text_inputs;
  }
}

void PhishingDOMFeatureExtractor::HandleScript(
    const blink::WebElement& element) {
  ++page_feature_state_->num_script_tags;
}

void PhishingDOMFeatureExtractor::RunCallback(bool success) {
  DCHECK(page_feature_state_.get());
  DCHECK(!done_callback_.is_null());
  TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "ExtractDomFeatures", this);
  std::move(done_callback_).Run(success);
  Clear();
}

void PhishingDOMFeatureExtractor::Clear() {
  features_ = nullptr;
  done_callback_.Reset();
  cur_frame_data_.reset(nullptr);
  cur_document_.Reset();
}

void PhishingDOMFeatureExtractor::ResetFrameData() {
  DCHECK(!cur_document_.IsNull());
  DCHECK(!cur_frame_data_.get());

  cur_frame_data_ = std::make_unique<FrameData>();
  cur_frame_data_->elements = cur_document_.All();
  cur_frame_data_->domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          cur_document_.Url(),
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

blink::WebDocument PhishingDOMFeatureExtractor::GetNextDocument() {
  DCHECK(!cur_document_.IsNull());
  blink::WebFrame* frame = cur_document_.GetFrame();
  // Advance to the next frame that contains a document, with no wrapping.
  if (frame) {
    for (frame = frame->TraverseNext(); frame; frame = frame->TraverseNext()) {
      // TODO(dcheng): Verify if the WebDocument::IsNull check is really needed.
      if (frame->IsWebLocalFrame() &&
          !frame->ToWebLocalFrame()->GetDocument().IsNull()) {
        return frame->ToWebLocalFrame()->GetDocument();
      }
    }
  }
  return blink::WebDocument();
}

bool PhishingDOMFeatureExtractor::IsExternalDomain(const GURL& url,
                                                   std::string* domain) const {
  DCHECK(domain);
  DCHECK(cur_frame_data_.get());

  if (cur_frame_data_->domain.empty()) {
    return false;
  }

  // TODO(bryner): Ensure that the url encoding is consistent with the features
  // in the model.
  if (url.HostIsIPAddress()) {
    domain->assign(url.host());
  } else {
    domain->assign(net::registry_controlled_domains::GetDomainAndRegistry(
        url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES));
  }

  return !domain->empty() && *domain != cur_frame_data_->domain;
}

blink::WebURL PhishingDOMFeatureExtractor::CompleteURL(
    const blink::WebElement& element,
    const blink::WebString& partial_url) {
  return element.GetDocument().CompleteURL(partial_url);
}

void PhishingDOMFeatureExtractor::InsertFeatures() {
  DCHECK(page_feature_state_.get());

  if (page_feature_state_->total_links > 0) {
    // Add a feature for the fraction of times the page links to an external
    // domain vs. an internal domain.
    double link_freq =
        static_cast<double>(page_feature_state_->external_links) /
        page_feature_state_->total_links;
    features_->AddRealFeature(features::kPageExternalLinksFreq, link_freq);

    // Add a feature for each unique domain that we're linking to
    for (const auto& domain : page_feature_state_->external_domains) {
      features_->AddBooleanFeature(features::kPageLinkDomain + domain);
    }

    // Fraction of links that use https.
    double secure_freq =
        static_cast<double>(page_feature_state_->secure_links) /
        page_feature_state_->total_links;
    features_->AddRealFeature(features::kPageSecureLinksFreq, secure_freq);
  }

  // Record whether forms appear and whether various form elements appear.
  if (page_feature_state_->num_forms > 0) {
    features_->AddBooleanFeature(features::kPageHasForms);
  }
  if (page_feature_state_->num_text_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasTextInputs);
  }
  if (page_feature_state_->num_pswd_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasPswdInputs);
  }
  if (page_feature_state_->num_radio_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasRadioInputs);
  }
  if (page_feature_state_->num_check_inputs > 0) {
    features_->AddBooleanFeature(features::kPageHasCheckInputs);
  }

  // Record fraction of form actions that point to a different domain.
  if (page_feature_state_->total_actions > 0) {
    double action_freq =
        static_cast<double>(page_feature_state_->action_other_domain) /
        page_feature_state_->total_actions;
    features_->AddRealFeature(features::kPageActionOtherDomainFreq,
                              action_freq);
  }

  // Add a feature for each unique external action url.
  for (const auto& url : page_feature_state_->page_action_urls) {
    features_->AddBooleanFeature(features::kPageActionURL + url);
  }

  // Record how many image src attributes point to a different domain.
  if (page_feature_state_->total_imgs > 0) {
    double img_freq =
        static_cast<double>(page_feature_state_->img_other_domain) /
        page_feature_state_->total_imgs;
    features_->AddRealFeature(features::kPageImgOtherDomainFreq, img_freq);
  }

  // Record number of script tags (discretized for numerical stability.)
  if (page_feature_state_->num_script_tags > 1) {
    features_->AddBooleanFeature(features::kPageNumScriptTagsGTOne);
    if (page_feature_state_->num_script_tags > 6) {
      features_->AddBooleanFeature(features::kPageNumScriptTagsGTSix);
    }
  }
}

}  // namespace safe_browsing
