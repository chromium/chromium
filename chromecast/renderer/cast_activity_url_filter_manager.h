// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_ACTIVITY_URL_FILTER_MANAGER_H_
#define CHROMECAST_RENDERER_CAST_ACTIVITY_URL_FILTER_MANAGER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chromecast/common/activity_url_filter.h"
#include "chromecast/common/mojom/activity_url_filter.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace chromecast {

// This class accepts strings that represent URL filter criteria from
// the browser through mojo, and constructs the URL matcher that filters
// web requests.
class CastActivityUrlFilterManager {
 public:
  CastActivityUrlFilterManager();

  CastActivityUrlFilterManager(const CastActivityUrlFilterManager&) = delete;
  CastActivityUrlFilterManager& operator=(const CastActivityUrlFilterManager&) =
      delete;

  ~CastActivityUrlFilterManager();

  // Returns nullptr if no Activity URL filter exists for the render frame.
  ActivityUrlFilter* GetActivityUrlFilterForRenderFrameToken(
      const blink::LocalFrameToken& frame_token);

  void OnRenderFrameCreated(content::RenderFrame* render_frame);
  void OnRenderFrameRemoved(const blink::LocalFrameToken& frame_token);

 private:
  class UrlFilterReceiver
      : public content::RenderFrameObserver,
        public chromecast::mojom::ActivityUrlFilterConfiguration {
   public:
    explicit UrlFilterReceiver(content::RenderFrame* render_frame,
                               base::OnceCallback<void()> on_removed_callback);

    UrlFilterReceiver(const UrlFilterReceiver&) = delete;
    UrlFilterReceiver& operator=(const UrlFilterReceiver&) = delete;

    ~UrlFilterReceiver() override;

    // mojom::ActivityUrlFilterConfiguration implementation:
    void SetFilter(
        chromecast::mojom::ActivityUrlFilterCriteriaPtr filter) override;

    ActivityUrlFilter* GetUrlFilter();

   private:
    // content::RenderFrameObserver implementation:
    bool OnAssociatedInterfaceRequestForFrame(
        const std::string& interface_name,
        mojo::ScopedInterfaceEndpointHandle* handle) override;
    void OnDestruct() override;

    void OnActivityUrlFilterConfigurationAssociatedRequest(
        mojo::PendingAssociatedReceiver<
            chromecast::mojom::ActivityUrlFilterConfiguration> receiver);

    std::unique_ptr<ActivityUrlFilter> url_filter_;

    blink::AssociatedInterfaceRegistry associated_interfaces_;
    mojo::AssociatedReceiverSet<
        chromecast::mojom::ActivityUrlFilterConfiguration>
        receivers_;

    base::OnceCallback<void()> on_removed_callback_;

    base::WeakPtr<UrlFilterReceiver> weak_this_;
    base::WeakPtrFactory<UrlFilterReceiver> weak_factory_;
  };

  base::flat_map<blink::LocalFrameToken, UrlFilterReceiver*>
      activity_url_filters_;

  base::WeakPtr<CastActivityUrlFilterManager> weak_this_;
  base::WeakPtrFactory<CastActivityUrlFilterManager> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_ACTIVITY_URL_FILTER_MANAGER_H_
