// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_text_observer.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
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

// PageTextChunkConsumer reads in chunks of page text and passes it all to
// the given callback, up to the maximum given length (in bytes).
// When the text reads have been completed (by either OnChunksEnd() or the given
// max length has been reached), the given callback |on_complete| is run and
// this class goes into an inactive state. The passed callback may delete |this|
// in stack.
class PageTextChunkConsumer : public mojom::PageTextConsumer {
 public:
  PageTextChunkConsumer(mojo::PendingReceiver<mojom::PageTextConsumer> receiver,
                        uint32_t max_size,
                        base::OnceCallback<void(base::string16)> on_complete)
      : remaining_size_(max_size),
        on_complete_(std::move(on_complete)),
        receiver_(this, std::move(receiver)) {
    // If any error occurs, just run |on_complete| with whatever text has been
    // received up to that point.
    receiver_.set_disconnect_handler(base::BindOnce(
        // base::Unretained is safe here since |receiver_| is owned by |this|
        // and mojo guarantees the passed callback won't be called on
        // |receiver_|'s destruction.
        &PageTextChunkConsumer::OnComplete, base::Unretained(this)));
  }
  ~PageTextChunkConsumer() override = default;

  // mojom::PageTextConsumer:
  void OnChunksEnd() override { OnComplete(); }
  void OnTextDumpChunk(const base::string16& chunk) override {
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

 private:
  // The maximum length in bytes that will be read from the data pipe.
  uint32_t remaining_size_ = 0;

  // While |on_complete_| is non-null, the mojo pipe is also bound. Once the
  // |on_complete_| callback is run, this class is no longer active and can be
  // deleted (in stack with the callback).
  base::OnceCallback<void(base::string16)> on_complete_;
  mojo::Receiver<mojom::PageTextConsumer> receiver_;

  // All chunks that have been read from the data pipe. These will be
  // concatenated together and passed to |on_complete_|.
  std::vector<base::string16> read_chunks_;

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
//
// (4) The PageTextConsumer will receive chunks of the dumped page text until
// either (a) all chunks have been read, or (b) the max length is reached. When
// this happens the text chunks received so far will be concatenated and passed
// back to RequestMediator in a callback.
//
// (5) The callback to RequestMediator is the end of the line for a text dump
// response as the original requests' callbacks are now passed the dumped
// string16. At this time, the self scoped_refptr will fall out of scope and be
// destroyed. If it was the last such pointer, then |this| will be destroyed.
class RequestMediator : public base::RefCounted<RequestMediator> {
 public:
  RequestMediator() = default;

  void AddConsumerRequest(
      std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request) {
    DCHECK(request->callback);
    DCHECK(!request->events.empty());
    DCHECK_GT(request->max_size, 0U);

    for (mojom::TextDumpEvent event : request->events) {
      if (max_size_by_event_.find(event) == max_size_by_event_.end()) {
        max_size_by_event_.emplace(event, 0U);
      }
      auto event_to_max_size_iter = max_size_by_event_.find(event);
      event_to_max_size_iter->second =
          std::max(event_to_max_size_iter->second, request->max_size);
    }

    requests_.push_back(std::move(request));
  }

  void MakeSelfOwnedAndDispatchRequests(
      scoped_refptr<RequestMediator> self,
      mojo::AssociatedRemote<mojom::PageTextService> renderer_text_service) {
    DCHECK_EQ(self.get(), this);

    for (const auto& event_to_max_size_iter : max_size_by_event_) {
      mojo::PendingRemote<mojom::PageTextConsumer> consumer_remote;

      std::unique_ptr<PageTextChunkConsumer> consumer =
          std::make_unique<PageTextChunkConsumer>(
              consumer_remote.InitWithNewPipeAndPassReceiver(),
              event_to_max_size_iter.second,
              // base::Unretained is safe here since the ownership of |this| is
              // passed to |consumer|. See comment at end of method for more
              // detail.
              base::BindOnce(&RequestMediator::OnPageTextAsString,
                             base::Unretained(this), self,
                             event_to_max_size_iter.first));

      auto request = mojom::PageTextDumpRequest::New();
      request->max_size = event_to_max_size_iter.second;
      request->event = event_to_max_size_iter.first;
      // TODO(crbug/1163244): Set this based on subframes.
      request->min_frame_pixel_area = 0;

      renderer_text_service->RequestPageTextDump(std::move(request),
                                                 std::move(consumer_remote));

      // |consumer| now owns |this| since it owns the callback with |self|, so
      // this is what makes this class "self owned". Once the consumer is done
      // reading text chunks (or is disconnected), then |OnPageTextAsString|
      // will be called where the scoped_refptr will fall out of scope and
      // destroy |this| (if it was the last owning reference).
      consumers_.emplace(std::move(consumer));
    }
  }

 private:
  friend class base::RefCounted<RequestMediator>;
  ~RequestMediator() = default;

  void OnPageTextAsString(scoped_refptr<RequestMediator> self,
                          mojom::TextDumpEvent event,
                          const base::string16 page_text) {
    for (const auto& request : requests_) {
      if (request->events.find(event) != request->events.end()) {
        request->callback.Run(page_text);
      }
    }
  }

  // All text dumps requests.
  std::vector<std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest>>
      requests_;

  // All |PageTextChunkConsumer|'s that are owned by this.
  std::set<std::unique_ptr<PageTextChunkConsumer>> consumers_;

  // The max length, in bytes, to request for each event.
  std::map<mojom::TextDumpEvent, uint32_t> max_size_by_event_;
};

}  // namespace

PageTextObserver::ConsumerTextDumpRequest::ConsumerTextDumpRequest() = default;
PageTextObserver::ConsumerTextDumpRequest::~ConsumerTextDumpRequest() = default;

PageTextObserver::PageTextObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}
PageTextObserver::~PageTextObserver() = default;

PageTextObserver* PageTextObserver::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  // CreateForWebContents doesn't do anything if it has already been created
  // for |web_contents| already.
  PageTextObserver::CreateForWebContents(web_contents);
  return PageTextObserver::FromWebContents(web_contents);
}

void PageTextObserver::DidFinishNavigation(content::NavigationHandle* handle) {
  if (consumers_.empty()) {
    return;
  }

  // Only main frames are supported for right now.
  // TODO(crbug/1163244): Add subframe support.
  if (!handle->IsInMainFrame()) {
    return;
  }

  if (!handle->HasCommitted()) {
    return;
  }

  content::RenderFrameHost* render_frame_host = handle->GetRenderFrameHost();

  scoped_refptr<RequestMediator> mediator =
      base::MakeRefCounted<RequestMediator>();
  for (Consumer* consumer : consumers_) {
    auto request = consumer->MaybeRequestFrameTextDump(handle);
    if (request) {
      mediator->AddConsumerRequest(std::move(request));
    }
  }

  mojo::AssociatedRemote<mojom::PageTextService> renderer_text_service;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &renderer_text_service);

  mediator->MakeSelfOwnedAndDispatchRequests(mediator,
                                             std::move(renderer_text_service));
}

void PageTextObserver::AddConsumer(Consumer* consumer) {
  consumers_.insert(consumer);
}

void PageTextObserver::RemoveConsumer(Consumer* consumer) {
  consumers_.erase(consumer);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PageTextObserver)

}  // namespace optimization_guide
