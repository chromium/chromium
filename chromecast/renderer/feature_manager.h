// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_FEATURE_MANAGER_H_
#define CHROMECAST_RENDERER_FEATURE_MANAGER_H_

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "chromecast/common/mojom/feature_manager.mojom.h"
#include "chromecast/renderer/native_bindings_helper.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "url/gurl.h"

namespace chromecast {

// Receives messages from the browser process to enable/disable Cast
// application-facing features. Features usually have an associated V8 binding
// which exposes a platform capability to the app.
class FeatureManager : public content::RenderFrameObserver,
                       public shell::mojom::FeatureManager {
 public:
  explicit FeatureManager(content::RenderFrame* render_frame);
  FeatureManager(const FeatureManager&) = delete;
  FeatureManager& operator=(const FeatureManager&) = delete;
  ~FeatureManager() override;

  const GURL& dev_origin() const { return dev_origin_; }
  bool configured() const { return configured_; }

  bool FeatureEnabled(const std::string& feature) const;

  const chromecast::shell::mojom::FeaturePtr& GetFeature(
      const std::string& feature) const;

  friend std::ostream& operator<<(std::ostream& os,
                                  const FeatureManager& features);

 protected:
  // Allows a derived class to add its own features at the end of
  // mojom::FeatureManager::ConfigureFeatures().
  virtual void ConfigureFeaturesInternal();

  // Map for storing enabled features, name -> FeaturePtr.
  using FeaturesMap =
      std::map<std::string, chromecast::shell::mojom::FeaturePtr>;
  FeaturesMap features_map_;

  base::flat_set<CastBinding*> v8_bindings_;

 private:
  // content::RenderFrameObserver implementation:
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  void DidClearWindowObject() override;
  void OnDestruct() override;

  // shell::mojom::FeatureManager implementation
  void ConfigureFeatures(
      std::vector<chromecast::shell::mojom::FeaturePtr> features) override;

  // Bind the incoming request with this implementation
  void OnFeatureManagerRequest(
      mojo::PendingReceiver<shell::mojom::FeatureManager> request);

  void EnableBindings();
  void SetupAdditionalSecureOrigin();

  // Flag for when the configuration message is received from the browser.
  bool configured_;
  bool can_install_bindings_;

  // Origin enabled for development
  GURL dev_origin_;
  bool secure_origin_set_;

  service_manager::BinderRegistry registry_;

  mojo::ReceiverSet<shell::mojom::FeatureManager> bindings_;
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_FEATURE_MANAGER_H_
