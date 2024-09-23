// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_FEATURE_MANAGER_ON_ASSOCIATED_INTERFACE_H_
#define CHROMECAST_RENDERER_FEATURE_MANAGER_ON_ASSOCIATED_INTERFACE_H_

#include <map>
#include <string>

#include "chromecast/renderer/feature_manager.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace chromecast {

// Similar to the internal FeatureManager, but it's attached to associated
// interface to ensure the actions are taken before the webpage is loaded.
// TODO(b/187758538): Refactor this class with the internal FeatureManager. The
// most likely result is we upstream and merge the internal FeatureManager to
// this class and rename this class to FeatureManager.
class FeatureManagerOnAssociatedInterface : public FeatureManager {
 public:
  explicit FeatureManagerOnAssociatedInterface(
      content::RenderFrame* render_frame);
  FeatureManagerOnAssociatedInterface(
      const FeatureManagerOnAssociatedInterface&) = delete;
  FeatureManagerOnAssociatedInterface& operator=(
      const FeatureManagerOnAssociatedInterface&) = delete;
  ~FeatureManagerOnAssociatedInterface() override;

 private:
  // FeautreManager implementation:
  void ConfigureFeaturesInternal() override;

  // Bind the incoming request with this implementation
  void OnFeatureManagerAssociatedRequest(
      mojo::PendingAssociatedReceiver<shell::mojom::FeatureManager>
          pending_receiver);

  // Flag for when the configuration message is received from the browser.
  bool configured_;

  // Map for storing enabled features, name -> FeaturePtr.
  using FeaturesMap =
      std::map<std::string, chromecast::shell::mojom::FeaturePtr>;
  FeaturesMap features_map_;

  blink::AssociatedInterfaceRegistry registry_;
  mojo::AssociatedReceiverSet<shell::mojom::FeatureManager> receivers_;
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_FEATURE_MANAGER_ON_ASSOCIATED_INTERFACE_H_
