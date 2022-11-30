// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_DEMO_BINDINGS_H_
#define CHROMECAST_RENDERER_CAST_DEMO_BINDINGS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chromecast/common/mojom/cast_demo.mojom.h"
#include "chromecast/renderer/native_bindings_helper.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "v8/include/v8.h"

namespace chromecast {
namespace shell {

// Enabled only when the device is in demonstration mode. This is only enabled
// for the demonstration JS app.
class CastDemoBindings : public CastBinding,
                         public mojom::CastDemoVolumeChangeObserver {
 public:
  explicit CastDemoBindings(content::RenderFrame* render_frame);
  CastDemoBindings(const CastDemoBindings&) = delete;
  CastDemoBindings& operator=(const CastDemoBindings&) = delete;

 private:
  friend class CastBinding;

  ~CastDemoBindings() override;

  // CastBinding implementation:
  void Install(v8::Local<v8::Object> cast_platform,
               v8::Isolate* isolate) override;

  // Methods to be called from v8: (See mojom for details)
  void RecordEvent(const std::string& event_name, v8::Local<v8::Value> data);
  void SetRetailerName(const std::string& retailer_name);
  void SetStoreId(const std::string& store_id);
  v8::Local<v8::Value> GetRetailerName();
  v8::Local<v8::Value> GetStoreId();
  void SetDefaultVolumeLevel(float level);
  v8::Local<v8::Value> GetDefaultVolumeLevel();
  void ApplyDefaultVolume();
  void SetWifiCredentials(const std::string& ssid, const std::string& psk);
  v8::Local<v8::Value> GetAvailableWifiNetworks();
  v8::Local<v8::Value> GetConnectionStatus();
  void SetVolumeChangeHandler(v8::Local<v8::Function> volume_change_handler);
  void PersistLocalStorage();

  // Deprecated
  void SetVolume(float level);

  // Methods to return values to v8:
  void OnGetRetailerName(v8::Global<v8::Promise::Resolver> resolver,
                         v8::Global<v8::Context> original_context,
                         const std::string& retailer_name);
  void OnGetStoreId(v8::Global<v8::Promise::Resolver> resolver,
                    v8::Global<v8::Context> original_context,
                    const std::string& store_id);
  void OnGetDefaultVolumeLevel(v8::Global<v8::Promise::Resolver> resolver,
                               v8::Global<v8::Context> original_context,
                               float level);
  void OnGetAvailableWifiNetworks(v8::Global<v8::Promise::Resolver> resolver,
                                  v8::Global<v8::Context> original_context,
                                  base::Value network_list);
  void OnGetConnectionStatus(v8::Global<v8::Promise::Resolver> resolver,
                             v8::Global<v8::Context> original_context,
                             base::Value status);

  // mojom::CastDemoVolumeChangeObserver implementation:
  void VolumeChanged(float level) override;

  void ReconnectMojo();
  void OnMojoConnectionError();

  // Returns a reference to |cast_demo_|, and binds it to a mojo pipe if
  // necessary.
  const mojo::Remote<mojom::CastDemo>& GetCastDemo();

  // The pointer to the remote mojom::CastDemo interface.  Do not access this
  // member directly; instead, use GetCastDemo().
  mojo::Remote<mojom::CastDemo> cast_demo_;

  mojo::Receiver<mojom::CastDemoVolumeChangeObserver> binding_;

  v8::UniquePersistent<v8::Function> volume_change_handler_;

  base::WeakPtrFactory<CastDemoBindings> weak_factory_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_DEMO_BINDINGS_H_
