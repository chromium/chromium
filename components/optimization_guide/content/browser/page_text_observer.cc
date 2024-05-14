// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_text_observer.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace optimization_guide {

namespace {

const char kTimeUntilDisconnectHistogram[] =
    "OptimizationGuide.PageTextDump.TimeUntilFrameDisconnected.";
const char kTimeUntilCompleteHistogram[] =
    "OptimizationGuide.PageTextDump.TimeUntilFrameDumpCompleted.";
const char kFrameDumpLengthHistogram[] =
    "OptimizationGuide.PageTextDump.FrameDumpLength.";

std::string TextDumpEventToString(mojom::TextDumpEvent event) {
  switch (event) {
    case mojom::TextDumpEvent::kFirstLayout:
      return "FirstLayout";
    case mojom::TextDumpEvent::kFinishedLoad:
      return "FinishedLoad";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

// PageTextChunkConsumer reads in chunks of page text and passes it all to
// the given callback, up to the maximum given length (in bytes).
// When the text reads have been completed (by either OnChunksEnd() or the given
// max length has been reached), the given callback |on_complete| is run and
// this class goes into an inactive state. The passed callback may delete |this|
// in stack. |on_complete_| will be called with nullopt if the mojo pipe was
// disconnected before a text dump finished.
class PageTextChunkConsumer : public mojom::PageTextConsumer {
 public:
  PageTextChunkConsumer(
      mojo::PendingReceiver<mojom::PageTextConsumer> receiver,
      uint32_t max_size,
      base::OnceCallback<void(const std::optional<std::u16string>&)>
          on_complete)
      : remaining_size_(max_size),
        on_complete_(std::move(on_complete)),
        receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        // base::Unretained is safe here since |receiver_| is owned by |this|
        // and mojo guarantees the passed callback won't be called on
        // |receiver_|'s destruction.
        &PageTextChunkConsumer::OnDisconnect, base::Unretained(this)));
  }
  ~PageTextChunkConsumer() override = default;

  // mojom::PageTextConsumer:
  void OnChunksEnd() override { OnComplete(); }
  void OnTextDumpChunk(const std::u16string& chunk) override {
    // Calling |OnComplete| will reset the mojo pipe and callback together, so
    // this method should not be called when there is not a callback.
    DCHECK(on_complete_);

    // Cap the number of bytes that will be read to the remaining max length. If
    // this occurs, run |OnComplete|.
    if (remaining_size_ <= chunk.size()) {
      read_chunks_.push_back(chunk.substr(0, remaining_size_));
      remaining_size_ = 0;
      OnComplete();
      // Do not add code here since |this| may be been destroyed in
      // |OnComplete|.
      return;
    }

    remaining_size_ -= chunk.size();
    read_chunks_.push_back(chunk);

    DCHECK_GT(remaining_size_, 0U);
  }

  void OnComplete() {
    receiver_.reset();
    std::move(on_complete_).Run(base::StrCat(read_chunks_));
    // Don't do anything else. This callback may have destroyed |this|.
  }

  void OnDisconnect() {
    receiver_.reset();
    std::move(on_complete_).Run(std::nullopt);
    // Don't do anything else. This callback may have destroyed |this|.
  }

 private:
  // The maximum length in bytes that will be read from the data pipe.
  uint32_t remaining_size_ = 0;

  // While |on_complete_| is non-null, the mojo pipe is also bound. Once the
  // |on_complete_| callback is run, this class is no longer active and can be
  // deleted (in stack with the callback).
  base::OnceCallback<void(const std::optional<std::u16string>&)> on_complete_;
  mojo::Receiver<mojom::PageTextConsumer> receiver_;

  // All chunks that have been read from the data pipe. These will be
  // concatenated together and passed to |on_complete_|.
  std::vector<std::u16string> read_chunks_;

  base::WeakPtrFactory<PageTextChunkConsumer> weak_factory_{this};
};

// RequestMediator handles the de-duplication of page text dump requests, and
// the multiplexes the response back to the callers.
//
// Lifetime: Once all the requests are known, the lifetime of this class is
// scoped to that of the associated remote. This is achieved by using a
// scoped_refptr which is copied onto every callback that is given to mojo. If
// the remote closes, the callbacks are destroyed, as will be this class.
// Otherwise, this class will remain alive until all mojo calls (and data pipe
// reads) have completed or are otherwise closed with an error.
//
// High Level Request Flow:
// (1) Request text dump from the renderer over the RenderFrameHost's associated
// remote interface. (2) The dumped text chunks come back and are concatenated
// back together. (3) The text dump, now in memory, is delivered in callbacks to
// all the original callers.

// Detailed Request Flow:
// (1) The caller will add some number of consumer requests thru
// |AddConsumerRequest|. Each request may give one or more events at which a
// text dump should be made, and the length that is being requested.
//
// (2) There may have been many requests to do a text dump at event Foo.
// However, only one dump request should be made of the renderer for that event
// since doing a dump is an expensive operation. Therefore, the greatest length
// for event Foo is determined and that length is what is requested from the
// renderer.
//
// (3) The caller will call |MakeSelfOwnedAndDispatchRequests| and pass in a
// scoped_refptr which owns |this|. At that time, all mojo requests are sent
// along with copies of the scoped_refptr, making mojo the effective owner of
// this class. Note that one or more requests have been made at this point and
// this class will remain alive until they have all been completed and handled.
// The count of outgoing requests sent to a renderer is returned.
//
// (4) The PageTextConsumer will receive chunks of the dumped page text until
// either (a) all chunks have been read, or (b) the max length is reached. When
// this happens the text chunks received so far will be concatenated and passed
// back to RequestMediator in a callback.
//
// (5) The callback to RequestMediator is the end of the line for a text dump
// response as the |on_complete_| callback is run. At this time, the self
// scoped_refptr will fall out of scope and be destroyed. If it was the last
// such pointer, then |this| will be destroyed.
class RequestMediator : public base::RefCounted<RequestMediator> {
 public:
  RequestMediator() = default;

  void AddConsumerRequest(
      const PageTextObserver::ConsumerTextDumpRequest& request) {
    DCHECK(!request.events.empty());
    DCHECK_GT(request.max_size, 0U);

    for (mojom::TextDumpEvent event : request.events) {
      if (max_size_by_event_.find(event) == max_size_by_event_.end()) {
        max_size_by_event_.emplace(event, 0U);
      }
      auto event_to_max_size_iter = max_size_by_event_.find(event);
      event_to_max_size_iter->second =
          std::max(event_to_max_size_iter->second, request.max_size);
    }
  }

  void AddAMPRequest(const PageTextObserver::ConsumerTextDumpRequest& request) {
    if (!request.dump_amp_subframes) {
      return;
    }

    auto iter = max_size_by_event_.find(mojom::TextDumpEvent::kFinishedLoad);
    if (iter == max_size_by_event_.end()) {
      max_size_by_event_.emplace(mojom::TextDumpEvent::kFinishedLoad,
                                 request.max_size);
      return;
    }

    iter->second = std::max(iter->second, request.max_size);
  }

  size_t MakeSelfOwnedAndDispatchRequests(
      scoped_refptr<RequestMediator> self,
      base::RepeatingCallback<void(std::optional<FrameTextDumpResult>)>
          on_frame_text_dump_complete,
      content::RenderFrameHost* rfh) {
    DCHECK_EQ(self.get(), this);

    if (max_size_by_event_.empty()) {
      return 0;
    }

    on_frame_text_dump_complete_ = std::move(on_frame_text_dump_complete);

    mojo::AssociatedRemote<mojom::PageTextService> renderer_text_service;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&renderer_text_service);

    auto rfh_id = rfh->GetGlobalId();
    bool is_subframe = rfh->GetMainFrame() != rfh;
    int nav_id = content::WebContents::FromRenderFrameHost(rfh)
                     ->GetController()
                     .GetVisibleEntry()
                     ->GetUniqueID();

    for (const auto& event_to_max_size_iter : max_size_by_event_) {
      mojo::PendingRemote<mojom::PageTextConsumer> consumer_remote;

      FrameTextDumpResult preliminary_result = FrameTextDumpResult::Initialize(
          event_to_max_size_iter.first, rfh_id,
          // Note that subframes only take text dumps iff they are an AMP
          // frame. If that even changes, this won't work anymore.
          /*amp_frame=*/is_subframe, nav_id);

      std::unique_ptr<PageTextChunkConsumer> consumer =
          std::make_unique<PageTextChunkConsumer>(
              consumer_remote.InitWithNewPipeAndPassReceiver(),
              event_to_max_size_iter.second,
              // base::Unretained is safe here since the ownership of |this| is
              // passed to |consumer|. See comment at end of method for more
              // detail.
              base::BindOnce(&RequestMediator::OnPageTextAsString,
                             base::Unretained(this), self, preliminary_result));

      auto request = mojom::PageTextDumpRequest::New();
      request->max_size = event_to_max_size_iter.second;
      request->event = event_to_max_size_iter.first;

      renderer_text_service->RequestPageTextDump(std::move(request),
                                                 std::move(consumer_remote));

      // |consumer| now owns |this| since it owns the callback with |self|, so
      // this is what makes this class "self owned". Once the consumer is done
      // reading text chunks (or is disconnected), then |OnPageTextAsString|
      // will be called where the scoped_refptr will fall out of scope and
      // destroy |this| (if it was the last owning reference).
      consumers_.emplace(std::move(consumer));
    }

    requests_sent_time_ = base::TimeTicks::Now();

    return max_size_by_event_.size();
  }

 private:
  friend class base::RefCounted<RequestMediator>;
  ~RequestMediator() = default;

  void OnPageTextAsString(scoped_refptr<RequestMediator> self,
                          const FrameTextDumpResult& preliminary_result,
                          const std::optional<std::u16string>& page_text) {
    DCHECK(on_frame_text_dump_complete_);

    std::string event_suffix =
        TextDumpEventToString(preliminary_result.event());

    if (!page_text) {
      base::UmaHistogramMediumTimes(
          kTimeUntilDisconnectHistogram + event_suffix,
          base::TimeTicks::Now() - requests_sent_time_);
      on_frame_text_dump_complete_.Run(std::nullopt);
      return;
    }

    base::UmaHistogramMediumTimes(kTimeUntilCompleteHistogram + event_suffix,
                                  base::TimeTicks::Now() - requests_sent_time_);

    base::UmaHistogramCounts10000(kFrameDumpLengthHistogram + event_suffix,
                                  page_text->size());

    on_frame_text_dump_complete_.Run(
        preliminary_result.CompleteWithContents(*page_text));
  }

  // Called whenever a text dump is completed for an event. This called as many
  // times as events requested, which can be greater than 1.
  base::RepeatingCallback<void(std::optional<FrameTextDumpResult>)>
      on_frame_text_dump_complete_;

  // All |PageTextChunkConsumer|'s that are owned by this.
  std::set<std::unique_ptr<PageTextChunkConsumer>> consumers_;

  // The max length, in bytes, to request for each event.
  std::map<mojom::TextDumpEvent, uint32_t> max_size_by_event_;

  // The time at which the mojo requests are sent, set during
  // |MakeSelfOwnedAndDispatchRequests|.
  base::TimeTicks requests_sent_time_;
};

}  // namespace

PageTextObserver::ConsumerTextDumpRequest::ConsumerTextDumpRequest() = default;
PageTextObserver::ConsumerTextDumpRequest::~ConsumerTextDumpRequest() = default;

PageTextObserver::PageTextObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PageTextObserver>(*web_contents) {}
PageTextObserver::~PageTextObserver() = default;

PageTextObserver* PageTextObserver::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  // CreateForWebContents doesn't do anything if it has already been created
  // for |web_contents| already.
  PageTextObserver::CreateForWebContents(web_contents);
  return PageTextObserver::FromWebContents(web_contents);
}

void PageTextObserver::DidFinishNavigation(content::NavigationHandle* handle) {
  // Only main frames are supported for right now.
  if (!handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (!handle->HasCommitted()) {
    return;
  }

  // Reset consumer requests if the navigation is not in the same document.
  if (!handle->IsSameDocument()) {
    requests_.clear();
    page_result_.reset();
    outstanding_requests_ = 0;
    outstanding_requests_grace_timer_.reset();
  }

  if (consumers_.empty()) {
    return;
  }

  scoped_refptr<RequestMediator> mediator =
      base::MakeRefCounted<RequestMediator>();
  for (Consumer* consumer : consumers_) {
    auto request = consumer->MaybeRequestFrameTextDump(handle);
    if (!request) {
      continue;
    }
    mediator->AddConsumerRequest(*request);
    requests_.push_back(std::move(request));
  }

  outstanding_requests_ += mediator->MakeSelfOwnedAndDispatchRequests(
      mediator,
      base::BindRepeating(&PageTextObserver::OnFrameTextDumpCompleted,
                          weak_factory_.GetWeakPtr()),
      handle->GetRenderFrameHost());
}

bool PageTextObserver::IsOOPIF(content::RenderFrameHost* rfh) const {
  return rfh->IsCrossProcessSubframe();
}

void PageTextObserver::RenderFrameCreated(content::RenderFrameHost* rfh) {
  if (!IsOOPIF(rfh) || !rfh->GetPage().IsPrimary()) {
    return;
  }

  scoped_refptr<RequestMediator> mediator =
      base::MakeRefCounted<RequestMediator>();
  for (const auto& request : requests_) {
    mediator->AddAMPRequest(*request);
  }

  outstanding_requests_ += mediator->MakeSelfOwnedAndDispatchRequests(
      mediator,
      base::BindRepeating(&PageTextObserver::OnFrameTextDumpCompleted,
                          weak_factory_.GetWeakPtr()),
      rfh);
}

void PageTextObserver::OnFrameTextDumpCompleted(
    std::optional<FrameTextDumpResult> frame_result) {
  // Ensure that the generated frame result is not for a previous page load.
  // This should be done before decrementing |outstanding_requests_| so that
  // each page load handles its own state.
  content::NavigationEntry* visible_entry =
      web_contents() ? web_contents()->GetController().GetVisibleEntry()
                     : nullptr;
  if (frame_result && visible_entry &&
      visible_entry->GetUniqueID() != frame_result->unique_navigation_id()) {
    return;
  }

  // |frame_result| will be null in the event the RFH dies, in which case we can
  // no longer expect the request to be fulfilled, so it should not be counted
  // as outstanding anymore.
  outstanding_requests_--;

  if (frame_result) {
    if (!page_result_) {
      page_result_ = std::make_unique<PageTextDumpResult>();
    }
    page_result_->AddFrameTextDumpResult(*frame_result);
  }

  if (!!outstanding_requests_grace_timer_ && outstanding_requests_ == 0) {
    outstanding_requests_grace_timer_.reset();
    DispatchResponses();
  }
}

void PageTextObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame())
    return;

  base::UmaHistogramCounts100(
      "OptimizationGuide.PageTextDump.OutstandingRequests.DidFinishLoad",
      outstanding_requests_);

  if (outstanding_requests_ > 0) {
    outstanding_requests_grace_timer_ = std::make_unique<base::OneShotTimer>();
    outstanding_requests_grace_timer_->Start(
        FROM_HERE, features::PageTextExtractionOutstandingRequestsGracePeriod(),
        base::BindOnce(&PageTextObserver::DispatchResponses,
                       base::Unretained(this)));
    return;
  }
  DispatchResponses();
}

void PageTextObserver::DispatchResponses() {
  outstanding_requests_grace_timer_.reset();

  base::UmaHistogramCounts100(
      "OptimizationGuide.PageTextDump.AbandonedRequests",
      outstanding_requests_);

  if (!page_result_) {
    return;
  }

  for (const auto& consumer_request : requests_) {
    std::move(consumer_request->callback).Run(*page_result_);
  }
  requests_.clear();
  page_result_.reset();
}

void PageTextObserver::AddConsumer(Consumer* consumer) {
  consumers_.insert(consumer);
}

void PageTextObserver::RemoveConsumer(Consumer* consumer) {
  consumers_.erase(consumer);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageTextObserver);

}  // namespace optimization_guide
